#!/usr/bin/env python3
"""Run, validate, and collect the Sarno backend matrix: build x MPI ranks x OpenMP threads x GPUs.

Read-only on forcing; each sample writes only below its own directory under the suite root.
`run` executes every (build, p, n, g) tuple once as a warm-up and once per replicate block in
rotated order, validates each sample against the serial-build 1/1/0 warm-up, and keeps
checksums instead of validated gridded output. `collect` summarizes valid samples and plots.
"""
import argparse
import datetime
import hashlib
import json
import math
import os
from pathlib import Path
import platform
import re
import shutil
import statistics
import subprocess
import sys
import time

sys.path.insert(0, str(Path(__file__).resolve().parent))
sys.path.insert(0, str(Path(__file__).resolve().parents[3] / "tools"))
from sarno_core_sweep import COMPONENTS, INTERVALS, accessible  # noqa: E402

REPOSITORY = Path(__file__).resolve().parents[3]
REFERENCE = ("serial", 1, 1, 0)
FAMILIES = {"serial": "Serial", "omp": "OpenMP", "mpi": "MPI", "mpi-omp": "MPI+OpenMP", "cuda": "CUDA",
            "omp-cuda": "OpenMP+CUDA", "mpi-cuda": "MPI+CUDA", "mpi-omp-cuda": "MPI+OpenMP+CUDA"}


def tuple_name(build, p, n, g):
    return f"{build}_p{p}_n{n}_g{g}"


def parse_config(text):
    build, p, n, g = text.split(":")
    if build not in FAMILIES:
        raise ValueError(f"unknown build {build}")
    p, n, g = int(p), int(n), int(g)
    if p < 1 or n < 1 or g < 0:
        raise ValueError(f"invalid resources in {text}")
    if p > 1 and "mpi" not in build or n > 1 and "omp" not in build or g > 0 and "cuda" not in build:
        raise ValueError(f"{text}: resources not provided by build {build}")
    return build, p, n, g


def sha256(path):
    digest = hashlib.sha256()
    with open(path, "rb") as stream:
        for block in iter(lambda: stream.read(1 << 22), b""):
            digest.update(block)
    return digest.hexdigest()


def prepare(root, rate, forcing, builds, nodes=1):
    """Write the suite provenance, configuration, and source once; verify the forcing checksums."""
    provenance = root / "provenance"
    if (provenance / "config.json").exists():
        return
    provenance.mkdir(parents=True, exist_ok=True)
    source = json.loads((REPOSITORY / "examples/sources-sarno_river/sources-sarno_river.json").read_text())
    if len(source["features"]) != 1:
        raise ValueError("Sarno matrix requires one declared source")
    source["features"][0]["properties"]["emission"] = {"mode": "uniform_rate", "rate": rate, "rate_unit": "particles/hour"}
    (provenance / "source.json").write_text(json.dumps(source, indent=2) + "\n")
    config = json.loads((REPOSITORY / "examples/wacomm-sarno-lite/wacomm-sarno-lite.json").read_text())
    config["io"].update(ocean_model="WaComM", base_path="processed-6h/", save_input=False,
                        nc_inputs=["ocm3_d03_20210701Z" + h + ".nc" for h in ("09", "10", "11", "13", "14", "15")])
    (provenance / "config.json").write_text(json.dumps(config, indent=2) + "\n")
    (provenance / "particles-per-hour.txt").write_text(f"{rate}\n")
    expected = {}
    for line in Path(forcing, "prepared-forcing.sha256").read_text().splitlines():
        digest, name = line.split()
        expected[Path(name).name] = digest
    actual = {name: sha256(Path(forcing, "processed-6h", name)) for name in expected}
    if actual != expected:
        raise ValueError("forcing checksums do not match prepared-forcing.sha256")
    shutil.copy(Path(forcing, "prepared-forcing.sha256"), provenance / "forcing.sha256")
    record = {"date_utc": datetime.datetime.now(datetime.timezone.utc).isoformat(), "host": platform.node(),
              "platform": platform.platform(), "machine": platform.machine(), "python": sys.version.split()[0], "nodes": nodes,
              "revision": subprocess.run(["git", "-C", str(REPOSITORY), "rev-parse", "HEAD"], capture_output=True,
                                         text=True).stdout.strip(),
              "working_tree_patch": subprocess.run(["git", "-C", str(REPOSITORY), "diff", "HEAD"], capture_output=True,
                                                   text=True).stdout, "builds": {}}
    for name, binary in builds.items():
        cache = Path(binary).parent / "CMakeCache.txt"
        record["builds"][name] = {"binary": str(binary), "binary_sha256": sha256(binary),
                                  "cmake_cache_sha256": sha256(cache) if cache.exists() else None}
        if cache.exists():
            shutil.copy(cache, provenance / f"CMakeCache-{name}.txt")
    for command, output in ((["lscpu"], "cpu.txt"), (["sysctl", "-a", "machdep.cpu", "hw"], "cpu.txt"),
                            (["nvidia-smi", "-q"], "gpu-query.txt"), (["mpirun", "--version"], "mpi.txt")):
        if shutil.which(command[0]) and not (provenance / output).exists():
            (provenance / output).write_text(subprocess.run(command, capture_output=True, text=True).stdout)
    (provenance / "suite.json").write_text(json.dumps(record, indent=2) + "\n")


def launch_command(build, p, n, binary, bind, environment=None, nodes=1):
    """Return the command line; MPI builds always go through mpirun so p=1 includes MPI start-up."""
    if "mpi" not in build:
        return [str(binary), "wacomm-sarno-lite-6h.json"]
    command = ["mpirun", "-np", str(p)]
    if nodes > 1:
        # Under Slurm, Open MPI 4 binds slot:PE=n against one CPU per remote daemon;
        # ppr places the same number of ranks on every node instead.
        if p % nodes:
            raise ValueError(f"{p} MPI processes do not divide evenly over {nodes} nodes")
        mapping = f"ppr:{p // nodes}:node"
        command += (["--map-by", f"{mapping}:PE={n}", "--bind-to", "core"] if bind
                    else ["--map-by", mapping, "--bind-to", "none"])
    elif bind:
        # One rank per n consecutive physical cores; OMP_PLACES=cores keeps its threads one per core.
        command += ["--map-by", f"slot:PE={n}", "--bind-to", "core"]
    else:
        command += ["--bind-to", "none"]
    for variable in ("OMP_NUM_THREADS", "OMP_PLACES", "OMP_PROC_BIND", "OMP_DYNAMIC", "CUDA_VISIBLE_DEVICES"):
        if environment is None or variable in environment:
            command += ["-x", variable]
    return command + [str(binary), "wacomm-sarno-lite-6h.json"]


def run_one(root, forcing, build, p, n, g, label, binary, bind, python, prune, nodes=1):
    sample = root / tuple_name(build, p, n, g) / f"sample-{label}"
    reference = root / tuple_name(*REFERENCE) / "sample-warmup"
    verdict = sample / "verdict.txt"
    if verdict.exists() and verdict.read_text().startswith("equivalence_passed=true"):
        return verdict.read_text().strip()
    shutil.rmtree(sample, ignore_errors=True)
    for directory in ("examples/sources-sarno_river", "output-6h", "snapshots-6h"):
        (sample / directory).mkdir(parents=True)
    (sample / "processed-6h").symlink_to(Path(forcing, "processed-6h").resolve())
    shutil.copy(root / "provenance/config.json", sample / "wacomm-sarno-lite-6h.json")
    shutil.copy(root / "provenance/source.json", sample / "examples/sources-sarno_river/sources-sarno_river.json")
    environment = dict(os.environ, OMP_NUM_THREADS=str(n), OMP_DYNAMIC="false",
                       CUDA_VISIBLE_DEVICES=",".join(str(i) for i in range(g)) if g else "-1")
    if bind:
        environment.update(OMP_PLACES="cores", OMP_PROC_BIND="close")
    command = launch_command(build, p, n, binary, bind, environment, nodes)
    (sample / "command.txt").write_text(" ".join(command) + "\n")
    start = time.monotonic()
    with open(sample / "stdout.txt", "w") as out, open(sample / "stderr.txt", "w") as err:
        process = subprocess.Popen(command, cwd=sample, env=environment, stdout=out, stderr=err)
        _, status, usage = os.wait4(process.pid, 0)
    elapsed = time.monotonic() - start
    code = os.waitstatus_to_exitcode(status)
    # ru_maxrss is bytes on macOS and KiB on Linux; for mpirun it is the largest child (one rank).
    maxrss = usage.ru_maxrss // 1024 if sys.platform == "darwin" else usage.ru_maxrss
    (sample / "time.txt").write_text(f"elapsed_seconds={elapsed:.3f} maxrss_kb={maxrss}\n")
    (sample / "exit.txt").write_text(f"{code}\n")
    result = "equivalence_passed=false nonzero exit"
    if code == 0:
        files = sorted(sample.glob("output-6h/*.nc")) + sorted(sample.glob("snapshots-6h/*.nc"))
        (sample / "outputs.sha256").write_text("".join(f"{sha256(f)}  {f.relative_to(sample)}\n" for f in files))
        check = subprocess.run([python, "-I", str(Path(__file__).resolve().parent / "sarno_core_sweep.py"), "validate",
                                str(reference), str(sample), str(g)], capture_output=True, text=True)
        result = (check.stdout.strip() or check.stderr.strip().splitlines()[-1]).replace("\n", " ")
        if sample.resolve() != reference.resolve():
            shutil.rmtree(sample / "output-6h")
            if prune and result.startswith("equivalence_passed=true"):
                shutil.rmtree(sample / "snapshots-6h")
    verdict.write_text(result + "\n")
    print(f"{datetime.datetime.now():%H:%M:%S} {tuple_name(build, p, n, g)} {label} exit={code} "
          f"elapsed={elapsed:.2f}s maxrss_kb={maxrss} {result}", flush=True)
    return result


def run(args):
    # Samples run in their own directories, so a relative binary path must be resolved here.
    builds = {name: Path(binary).resolve() for name, binary in (item.split("=", 1) for item in args.build)}
    unusable = sorted(name for name, binary in builds.items() if not os.access(binary, os.X_OK))
    if unusable:
        raise ValueError(f"no executable binary for builds {unusable}")
    configs = [parse_config(c) for c in args.config]
    if REFERENCE not in configs:
        configs.insert(0, REFERENCE)
    missing = {c[0] for c in configs} - set(builds)
    if missing:
        raise ValueError(f"no binary for builds {sorted(missing)}")
    root = args.root.resolve()
    if args.nodes < 1:
        raise ValueError("--nodes must be positive")
    prepare(root, args.rate, args.forcing, builds, args.nodes)
    common = (root, args.forcing, args.bind, args.python, args.prune_snapshots, args.nodes)

    def one(config, label):
        build, p, n, g = config
        return run_one(common[0], common[1], build, p, n, g, label, builds[build], *common[2:])

    if not one(REFERENCE, "warmup").startswith("equivalence_passed=true"):
        raise ValueError("reference run failed")
    for config in configs:
        one(config, "warmup")
    for block in range(1, args.blocks + 1):
        offset = (block - 1) * 5 % len(configs)
        for index in range(len(configs)):
            one(configs[(index + offset) % len(configs)], str(block))
    print("DONE", flush=True)


def solver_intervals(stdout, p, n, g, build):
    matches = re.findall(r"Solver interval: start=([^ ]+) end=([^ ]+) seconds=([^\s]+)", stdout)
    values = [(float(a), float(b), float(c)) for a, b, c in matches]
    if [(a, b) for a, b, _ in values] != list(zip(INTERVALS[:-1], INTERVALS[1:])):
        raise ValueError("missing, duplicate, or reordered solver intervals")
    if any(not math.isfinite(t) or t <= 0 for _, _, t in values):
        raise ValueError("invalid solver duration")
    if f"Using 1/{p} processes, each on {n} threads." not in stdout:
        raise ValueError("runtime process or thread count differs from the tuple")
    devices = re.findall(r"Acceleration: CUDA (\d+) device\(s\)", stdout)
    if "cuda" in build and devices != [str(g)] or "cuda" not in build and devices:
        raise ValueError(f"expected {g} visible CUDA devices")
    return [t for _, _, t in values]


def suite(root):
    """Collect medians of valid measured samples for every tuple of one suite."""
    root = Path(root)
    rows, reference = [], root / tuple_name(*REFERENCE) / "sample-warmup"
    for directory in sorted(root.glob("*_p*_n*_g*")):
        build, p, n, g = re.fullmatch(r"(.+)_p(\d+)_n(\d+)_g(\d+)", directory.name).groups()
        p, n, g = int(p), int(n), int(g)
        solver, application, memory, parts, rejected, particles = [], [], [], [], [], []
        for sample in sorted(directory.glob("sample-[0-9]*")):
            try:
                if (sample / "exit.txt").read_text().strip() != "0":
                    raise ValueError("nonzero exit")
                report = json.loads((sample / "validation.json").read_text())
                if not report["equivalence_passed"] or Path(report["reference"]).resolve() != reference.resolve():
                    raise ValueError(report.get("error", "wrong reference"))
                stdout = (sample / "stdout.txt").read_text()
                solver.append(math.fsum(solver_intervals(stdout, p, n, g, build)))
                timing = (sample / "time.txt").read_text()
                application.append(float(re.search(r"elapsed_seconds=([0-9.]+)", timing).group(1)))
                memory.append(int(re.search(r"maxrss_kb=(\d+)", timing).group(1)))
                parts.append({k: math.fsum(float(x) for x in re.findall(v, stdout)) for k, v in COMPONENTS.items()})
                particles = [int(x) for x in re.findall(r"Total particles: (\d+)", stdout)]
            except (ValueError, OSError, KeyError, AttributeError) as error:
                rejected.append(f"{sample.name}: {error}")
        if not solver:
            raise ValueError(f"{directory.name}: no valid samples ({'; '.join(rejected)})")
        rows.append({"build": build, "family": FAMILIES[build], "mpi_processes": p, "openmp_threads": n, "gpus": g,
                     "cpu_workers": p * n, "solver_seconds": solver, "application_seconds": application,
                     "solver_median": statistics.median(solver), "solver_range": [min(solver), max(solver)],
                     "application_median": statistics.median(application),
                     "max_rss_kib_median": statistics.median(memory),
                     "components_median": {k: statistics.median(x[k] for x in parts) for k in COMPONENTS},
                     "particles_per_interval": particles, "rejected": rejected})
    base = next(r for r in rows if (r["build"], r["mpi_processes"], r["openmp_threads"], r["gpus"]) == REFERENCE)
    for row in rows:
        row["speedup"] = base["solver_median"] / row["solver_median"]
        row["application_speedup"] = base["application_median"] / row["application_median"]
        row["cpu_efficiency"] = row["speedup"] / row["cpu_workers"] if row["gpus"] == 0 else None
    rows.sort(key=lambda r: (list(FAMILIES).index(r["build"]), r["gpus"], r["mpi_processes"], r["openmp_threads"]))
    provenance = json.loads((root / "provenance/suite.json").read_text())
    return {"root": str(root), "particles_per_hour": int((root / "provenance/particles-per-hour.txt").read_text()),
            "host": provenance["host"], "nodes": provenance.get("nodes", 1), "platform": provenance["platform"], "revision": provenance["revision"],
            "working_tree_patch_bytes": len(provenance["working_tree_patch"]),
            "baseline_solver_seconds": base["solver_median"], "baseline_application_seconds": base["application_median"],
            "rows": rows}


def best(rows, key, gpus=None):
    """Smallest median; ties go to fewer GPUs, ranks, then threads."""
    candidates = [r for r in rows if gpus is None or (r["gpus"] > 0) == gpus]
    return min(candidates, key=lambda r: (r[key], r["gpus"], r["mpi_processes"], r["openmp_threads"])) if candidates else None


def label(row):
    return f"{row['family']} {row['mpi_processes']}×{row['openmp_threads']}" + (f" +{row['gpus']}GPU" if row["gpus"] else "")


# Validated categorical slots (light surface); serial is the neutral reference. Every bar is also
# direct-labelled with its tuple, so identity never depends on colour alone.
GROUPS = {"Serial": "#8a8985", "OpenMP": "#2a78d6", "MPI / MPI+OpenMP": "#eb6834", "CUDA (any host)": "#1baf7a"}


def slug(text):
    return re.sub(r"[^a-z0-9]+", "-", text.lower()).strip("-")


def group(row):
    if row["gpus"]:
        return "CUDA (any host)"
    if row["mpi_processes"] > 1 or "mpi" in row["build"]:
        return "MPI / MPI+OpenMP"
    return "OpenMP" if row["build"] == "omp" else "Serial"


def figures(suites, output):
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    from matplotlib.patches import Patch
    plt.rcParams.update({"font.size": 9, "axes.edgecolor": "#52514e", "axes.labelcolor": "#0b0b0b",
                         "xtick.color": "#52514e", "ytick.color": "#52514e", "svg.fonttype": "none",
                         "figure.facecolor": "white", "axes.facecolor": "white"})
    paths = []

    def save(figure, name, title, description):
        path = output / name
        figure.savefig(path, format="svg", metadata={"Date": None}, bbox_inches="tight")
        plt.close(figure)
        accessible(path, title, description)
        paths.append(path)

    def recessive(axis):
        axis.grid(True, axis="x", which="both", color="#e4e3df", linewidth=0.6)
        axis.set_axisbelow(True)
        for side in ("top", "right"):
            axis.spines[side].set_visible(False)

    for name, data in suites.items():
        rows = sorted(data["rows"], key=lambda r: r["solver_median"], reverse=True)
        figure, axis = plt.subplots(figsize=(7.2, 0.26 * len(rows) + 1.2))
        y = range(len(rows))
        axis.barh(y, [r["solver_median"] for r in rows], height=0.72, color=[GROUPS[group(r)] for r in rows],
                  edgecolor="white", linewidth=1)
        for i, r in zip(y, rows):
            axis.text(r["solver_median"] * 1.04, i, f"{r['solver_median']:.2f} s  ({r['speedup']:.1f}×)",
                      va="center", fontsize=7.5, color="#52514e")
        axis.set_yticks(list(y), [label(r) for r in rows], fontsize=7.5)
        axis.set_xscale("log")
        axis.set_xlim(right=max(r["solver_median"] for r in rows) * 3)
        axis.set_xlabel("median solver time over five forcing intervals (s, log scale)")
        axis.set_title(f"{name}: Sarno 6 h, {data['particles_per_hour']:,} particles/hour", loc="left")
        axis.legend(handles=[Patch(color=c, label=g) for g, c in GROUPS.items() if any(group(r) == g for r in rows)],
                    loc="upper right", fontsize=7.5, frameon=False)
        recessive(axis)
        save(figure, f"solver-time-{slug(name)}.svg", f"Solver time of every backend tuple, {name}",
             f"Horizontal bars of median solver time on a log scale for every validated build, MPI rank, OpenMP thread, "
             f"and GPU tuple on {name} at {data['particles_per_hour']} particles per hour, fastest at the top, "
             "labelled with the median and the speedup over the serial build. Computational diagnostic of measured "
             "execution; no confidence intervals.")

    cpu = {n: d for n, d in suites.items() if any(r["gpus"] == 0 and r["cpu_workers"] > 1 for r in d["rows"])}
    if cpu:
        figure, axes = plt.subplots(1, len(cpu), figsize=(4.6 * len(cpu), 3.8), squeeze=False)
        for axis, (name, data) in zip(axes[0], cpu.items()):
            rows = [r for r in data["rows"] if r["gpus"] == 0]
            top = max(r["cpu_workers"] for r in rows)
            axis.plot([1, top], [1, top], ":", color="#8a8985", linewidth=1.5, label="ideal (p·n)")
            series = (("OpenMP threads (p=1)", "#2a78d6", "o", [r for r in rows if r["build"] == "omp"]),
                      ("MPI ranks (n=1)", "#eb6834", "s", [r for r in rows if r["build"] == "mpi"]),
                      ("MPI×OpenMP hybrid", "#1baf7a", "D", [r for r in rows if r["build"] == "mpi-omp"]))
            for text, color, marker, points in series:
                points = sorted(points, key=lambda r: (r["cpu_workers"], r["mpi_processes"]))
                if not points:
                    continue
                style = "-" if text != "MPI×OpenMP hybrid" else "none"
                axis.plot([r["cpu_workers"] for r in points], [r["speedup"] for r in points], linestyle=style,
                          marker=marker, markersize=6, color=color, linewidth=2, label=text,
                          markeredgecolor="white", markeredgewidth=1)
                if text == "MPI×OpenMP hybrid":
                    # Placements with the same worker count share an x position: stack their labels to the right.
                    for workers in sorted({r["cpu_workers"] for r in points}):
                        same = sorted((r for r in points if r["cpu_workers"] == workers), key=lambda r: -r["speedup"])
                        for index, r in enumerate(same):
                            axis.annotate(f"{r['mpi_processes']}×{r['openmp_threads']} ({r['speedup']:.1f}×)",
                                          (r["cpu_workers"], r["speedup"]), textcoords="offset points",
                                          xytext=(8, 4 - 10 * index), fontsize=7, color="#52514e",
                                          arrowprops={"arrowstyle": "-", "color": "#c3c2b7", "linewidth": 0.6})
            axis.set_xlim(right=top * 1.35)
            axis.set_xlabel("CPU workers p·n (dimensionless)")
            axis.set_ylabel("solver speedup over serial build (dimensionless)")
            axis.set_title(f"{name}, {data['particles_per_hour']:,} particles/hour", loc="left")
            axis.grid(True, color="#e4e3df", linewidth=0.6)
            for side in ("top", "right"):
                axis.spines[side].set_visible(False)
            axis.legend(fontsize=7.5, frameon=False)
        figure.tight_layout()
        save(figure, "cpu-speedup.svg", "CPU strong-scaling speedup by decomposition",
             "Solver speedup relative to the serial build against CPU workers for the OpenMP thread sweep, the MPI rank "
             "sweep, and labelled MPI-by-OpenMP hybrid placements, with the ideal line, one panel per machine. "
             "Computational diagnostic; no confidence intervals.")

    figure, axes = plt.subplots(1, len(suites), figsize=(4.2 * len(suites), 3.6), squeeze=False)
    for axis, (name, data) in zip(axes[0], suites.items()):
        picks = []
        for candidate in (best([r for r in data["rows"] if r["build"] == b], "solver_median")
                          for b in FAMILIES):
            if candidate:
                picks.append(candidate)
        picks.sort(key=lambda r: r["application_median"], reverse=True)
        y = range(len(picks))
        solver = [r["solver_median"] for r in picks]
        rest = [max(r["application_median"] - r["solver_median"], 0) for r in picks]
        axis.barh(y, solver, height=0.7, color="#2a78d6", edgecolor="white", linewidth=1, label="solver intervals")
        axis.barh(y, rest, left=solver, height=0.7, color="#c3c2b7", edgecolor="white", linewidth=1,
                  label="start-up, forcing input, output")
        for i, r in zip(y, picks):
            axis.text(r["application_median"] + 0.3, i, f"{r['application_median']:.1f} s", va="center", fontsize=7.5,
                      color="#52514e")
        axis.set_yticks(list(y), [label(r) for r in picks], fontsize=7.5)
        axis.set_xlabel("median application wall time (s)")
        axis.set_title(f"{name}, {data['particles_per_hour']:,} p/h", loc="left")
        axis.set_xlim(right=max(r["application_median"] for r in picks) * 1.25)
        axis.legend(fontsize=7.5, frameon=False, loc="upper center", bbox_to_anchor=(0.5, -0.2), ncol=2)
        recessive(axis)
    figure.tight_layout()
    save(figure, "application-breakdown.svg", "Application time split for the best tuple of each build",
         "Stacked horizontal bars of median solver time and remaining application time for the fastest solver tuple "
         "of each build, one panel per suite. The remainder is application wall time minus solver time and includes "
         "start-up, forcing input, and output. Computational diagnostic; no confidence intervals.")
    return paths


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    commands = parser.add_subparsers(dest="command", required=True)
    execute = commands.add_parser("run", help="execute and validate a suite")
    execute.add_argument("--root", type=Path, required=True)
    execute.add_argument("--rate", type=int, required=True, help="particles/hour from the single Sarno source")
    execute.add_argument("--blocks", type=int, default=3)
    execute.add_argument("--forcing", type=Path, required=True, help="directory with processed-6h/ and prepared-forcing.sha256")
    execute.add_argument("--build", action="append", required=True, metavar="NAME=BINARY")
    execute.add_argument("--config", action="append", required=True, metavar="BUILD:P:N:G")
    execute.add_argument("--bind", action="store_true", help="bind ranks and threads to physical cores (Linux)")
    execute.add_argument("--nodes", type=int, default=1,
                         help="nodes of the allocation; MPI ranks are spread evenly with ppr placement")
    execute.add_argument("--python", default=sys.executable)
    execute.add_argument("--prune-snapshots", action="store_true")
    collect = commands.add_parser("collect", help="summarize validated suites")
    collect.add_argument("--suite", action="append", required=True, metavar="LABEL=ROOT")
    collect.add_argument("--output", type=Path, required=True)
    collect.add_argument("--no-figures", action="store_true")
    args = parser.parse_args()
    try:
        if args.command == "run":
            run(args)
            return
        suites = {}
        for item in args.suite:
            name, root = item.split("=", 1)
            suites[name] = suite(root)
        args.output.mkdir(parents=True, exist_ok=True)
        for data in suites.values():
            data["best_solver"] = label(best(data["rows"], "solver_median"))
            data["best_cpu_solver"] = label(best(data["rows"], "solver_median", gpus=False))
            data["best_application"] = label(best(data["rows"], "application_median"))
        result = {"schema": "wacomm-sarno-backend-matrix-v1",
                  "estimator": "median of valid measured samples; solver = sum of five solver intervals timed on rank 0 "
                               "between MPI barriers (s); application = launcher wall time (s)", "suites": suites}
        if not args.no_figures:
            result["figures"] = [p.name for p in figures(suites, args.output)]
        (args.output / "results.json").write_text(json.dumps(result, indent=2) + "\n")
        for name, data in suites.items():
            print(f"## {name} ({data['host']}), {data['particles_per_hour']} particles/hour")
            print("| build | p | n | g | samples | median solver (s) | range (s) | speedup | CPU efficiency | "
                  "median application (s) | max RSS/rank (GiB) |")
            print("| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |")
            for r in data["rows"]:
                efficiency = f"{100 * r['cpu_efficiency']:.1f}%" if r["cpu_efficiency"] is not None else "–"
                print(f"| {r['family']} | {r['mpi_processes']} | {r['openmp_threads']} | {r['gpus']} | "
                      f"{len(r['solver_seconds'])} | {r['solver_median']:.3f} | {r['solver_range'][0]:.3f}–"
                      f"{r['solver_range'][1]:.3f} | {r['speedup']:.2f}× | {efficiency} | {r['application_median']:.2f} | "
                      f"{r['max_rss_kib_median'] / 2**20:.2f} |")
            print(f"\nFastest solver: {data['best_solver']}; fastest CPU-only solver: {data['best_cpu_solver']}; "
                  f"fastest application: {data['best_application']}\n")
    except (ValueError, OSError, KeyError) as error:
        parser.exit(2, f"{error}\n")


if __name__ == "__main__":
    main()
