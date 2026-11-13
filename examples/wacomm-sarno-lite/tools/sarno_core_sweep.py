#!/usr/bin/env python3
"""Validate and collect the Sarno one-process physical-core CPU/GPU sweep (read-only on simulation output)."""
import argparse
import datetime
import json
import math
from pathlib import Path
import re
import statistics
import sys
import traceback

sys.path.insert(0, str(Path(__file__).resolve().parents[3] / "tools"))

TIMES = [datetime.datetime(2021, 7, 1, h, tzinfo=datetime.timezone.utc) for h in (9, 10, 11, 13, 14, 15)]
EPOCH = datetime.datetime(1968, 5, 23, tzinfo=datetime.timezone.utc)
INTERVALS = [(t - EPOCH).total_seconds() for t in TIMES]
REFERENCE = Path("p1_n1_g0") / "sample-warmup"
COMPONENTS = {"host_to_device_seconds": r"CUDA HOST2DEV sec: ([0-9.eE+-]+)",
              "gpu_section_seconds": r"GPU Processed in sec: ([0-9.eE+-]+)",
              "concentration_seconds": r"EVALUATE sec: ([0-9.eE+-]+)",
              # Rank-0 MPI distribution and collection, from revision e2c9a21; zero before and without MPI
              "distribute_seconds": r"DISTRIBUTE sec: ([0-9.eE+-]+)",
              "collect_seconds": r"COLLECT sec: ([0-9.eE+-]+)",
              # Rank-0 run phases (forcing preparation with the slowest rank's, setup, output, flush, whole run); zero before they were logged
              "forcing_seconds": r"FORCING sec: ([0-9.eE+-]+) max:",
              "forcing_max_seconds": r"FORCING sec: [0-9.eE+-]+ max: ([0-9.eE+-]+)",
              # Rank-0 parts of forcing preparation (NetCDF lock wait, ocean reads, ocean normalization,
              # boundary records), from the revision after 7242206; zero before
              "forcing_wait_seconds": r"FORCING sec: .* wait: ([0-9.eE+-]+)",
              "forcing_read_seconds": r"FORCING sec: .* read: ([0-9.eE+-]+)",
              "forcing_normalize_seconds": r"FORCING sec: .* normalize: ([0-9.eE+-]+)",
              "forcing_boundary_seconds": r"FORCING sec: .* boundary: ([0-9.eE+-]+)",
              "setup_seconds": r"SETUP sec: ([0-9.eE+-]+)",
              "output_seconds": r"OUTPUT sec: ([0-9.eE+-]+)",
              "flush_seconds": r"FLUSH sec: ([0-9.eE+-]+)",
              "run_seconds": r"RUN sec: ([0-9.eE+-]+)"}


def validate(reference, candidate, gpus):
    """Compare one sample with the 1/1/0 reference; write validation.json; return the verdict."""
    from compare_particle_snapshots import compare
    from performance_evidence import gpu_compare, gridded_compare
    report = {"reference": str(reference), "candidate": str(candidate), "gpus": gpus}
    try:
        if reference.resolve() == candidate.resolve():
            report["particles"] = report["gridded"] = "reference sample"
        else:
            report["particles"] = (gpu_compare if gpus else compare)(reference / "snapshots-6h", candidate / "snapshots-6h")
            report["gridded"] = gridded_compare(reference / "output-6h", candidate / "output-6h")
        report["equivalence_passed"] = True
    except Exception as error:
        report["equivalence_passed"] = False
        report["error"] = "".join(traceback.format_exception_only(type(error), error)).strip()
    (candidate / "validation.json").write_text(json.dumps(report, indent=2) + "\n")
    return report


def solver_intervals(stdout, threads, gpus):
    """Return the five solver-interval durations (s) after checking order and runtime resources."""
    matches = re.findall(r"Solver interval: start=([^ ]+) end=([^ ]+) seconds=([^\s]+)", stdout)
    values = [(float(a), float(b), float(c)) for a, b, c in matches]
    if [(a, b) for a, b, _ in values] != list(zip(INTERVALS[:-1], INTERVALS[1:])):
        raise ValueError("missing, duplicate, or reordered solver intervals")
    if any(not math.isfinite(t) or t <= 0 for _, _, t in values):
        raise ValueError("invalid solver duration")
    if f"Using 1/1 processes, each on {threads} threads." not in stdout:
        raise ValueError("wrong runtime thread count")
    devices = re.findall(r"Acceleration: CUDA (\d+) device\(s\)", stdout)
    if devices != [str(gpus)]:
        raise ValueError(f"expected {gpus} visible CUDA devices")
    return [t for _, _, t in values]


def physical_cores(stdout, threads):
    """Return the logical CPUs reported by OMP_DISPLAY_AFFINITY; require one per thread."""
    cpus = sorted(int(x) for x in re.findall(r"^level 1 thread 0x[0-9a-f]+ affinity (\d+)$", stdout, re.MULTILINE))
    if threads == 1 and not cpus:
        # The OpenMP runtime reports no affinity for a single-thread team; one thread cannot share a core.
        return []
    if len(cpus) != threads or len(set(cpus)) != threads:
        raise ValueError("threads are not bound to distinct logical CPUs")
    return cpus


def suite(root):
    """Collect one suite: medians of valid samples per (threads, GPUs) tuple."""
    root = Path(root)
    core_of = {}
    for line in (root / "provenance" / "cpu-topology.txt").read_text().splitlines()[1:]:
        cpu, core = line.split()[:2]
        core_of[int(cpu)] = int(core)
    rows = []
    for run in sorted(root.glob("p1_n*_g*"), key=lambda p: tuple(int(x) for x in re.findall(r"\d+", p.name))):
        _, threads, gpus = (int(x) for x in re.findall(r"\d+", run.name))
        solver, application, parts, rejected, particles = [], [], [], [], []
        for sample in sorted(run.glob("sample-[0-9]*")):
            try:
                if (sample / "exit.txt").read_text().strip() != "0":
                    raise ValueError("nonzero exit")
                report = json.loads((sample / "validation.json").read_text())
                if not report["equivalence_passed"] or Path(report["reference"]).resolve() != (root / REFERENCE).resolve():
                    raise ValueError(report.get("error", "wrong reference"))
                stdout = (sample / "stdout.txt").read_text()
                text = stdout + (sample / "stderr.txt").read_text()
                cpus = physical_cores(text, threads)
                if cpus and len({core_of[c] for c in cpus}) != threads:
                    raise ValueError("two threads share a physical core")
                solver.append(math.fsum(solver_intervals(stdout, threads, gpus)))
                application.append(float(re.search(r"elapsed_seconds=([0-9.]+)", (sample / "time.txt").read_text()).group(1)))
                parts.append({k: math.fsum(float(x) for x in re.findall(p, stdout)) for k, p in COMPONENTS.items()})
                particles = [int(x) for x in re.findall(r"Total particles: (\d+)", stdout)]
            except (ValueError, OSError, KeyError) as error:
                rejected.append(f"{sample.name}: {error}")
        if not solver:
            raise ValueError(f"{run.name}: no valid samples ({'; '.join(rejected)})")
        rows.append({"threads": threads, "gpus": gpus, "solver_seconds": solver, "application_seconds": application,
                     "solver_median": statistics.median(solver), "solver_range": [min(solver), max(solver)],
                     "application_median": statistics.median(application),
                     "components_median": {k: statistics.median(p[k] for p in parts) for k in COMPONENTS},
                     "particles_per_interval": particles, "rejected": rejected})
    base = next(r for r in rows if r["threads"] == 1 and r["gpus"] == 0)["solver_median"]
    for row in rows:
        row["speedup"] = base / row["solver_median"]
        row["cpu_efficiency"] = row["speedup"] / row["threads"] if row["gpus"] == 0 else None
        same = next((r for r in rows if r["gpus"] == 0 and r["threads"] == row["threads"]), None)
        row["gpu_speedup_same_cores"] = same["solver_median"] / row["solver_median"] if row["gpus"] and same else None
    provenance = root / "provenance"
    return {"root": str(root), "particles_per_hour": int((provenance / "particles-per-hour.txt").read_text()),
            "revision": (provenance / "binary-revision.txt").read_text().strip(),
            "working_tree_patch_bytes": (provenance / "binary-working-tree.patch").stat().st_size,
            "baseline_solver_seconds": base, "rows": rows}


def accessible(path, title, description):
    text = path.read_text()
    start = text.index(">", text.index("<svg")) + 1
    path.write_text(text[:start] + f"\n<title>{title}</title>\n<desc>{description}</desc>" + text[start:])


def figures(suites, output):
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    rates = sorted({s["particles_per_hour"] for s in suites.values()})
    styles = {0: ("CPU", "-", "o"), 1: ("1 GPU", "--", "s")}
    colors = dict(zip(sorted({label.split(":")[0] for label in suites}), ("#4c72b0", "#dd8452", "#55a868", "#c44e52")))
    figure, axes = plt.subplots(1, len(rates), figsize=(5.2 * len(rates), 4.2), squeeze=False)
    for axis, rate in zip(axes[0], rates):
        for label, data in suites.items():
            if data["particles_per_hour"] != rate:
                continue
            build = label.split(":")[0]
            for gpus, (name, line, marker) in styles.items():
                rows = [r for r in data["rows"] if r["gpus"] == gpus]
                if rows:
                    axis.plot([r["threads"] for r in rows], [r["solver_median"] for r in rows], line, marker=marker,
                              color=colors[build], label=f"{build}, {name}")
        axis.set_yscale("log")
        axis.set_xticks(range(1, 13))
        axis.set_xlabel("OpenMP threads, one per physical core (dimensionless)")
        axis.set_ylabel("median solver time (s)")
        axis.set_title(f"{rate:,} particles/hour")
        axis.grid(True, which="both", alpha=0.3)
        axis.legend(fontsize=8)
    figure.tight_layout()
    time_path = output / "solver-time.svg"
    figure.savefig(time_path, format="svg", metadata={"Date": None})
    plt.close(figure)
    accessible(time_path, "Sarno solver time against physical cores with and without one GPU",
               "Median of three five-interval solver times on a log scale against 1 to 12 OpenMP threads pinned to "
               "distinct physical cores, for each release rate, build, and zero or one visible GPU. "
               "Computational diagnostic; no confidence intervals.")
    figure, axes = plt.subplots(1, len(rates), figsize=(5.2 * len(rates), 4.2), squeeze=False)
    for axis, rate in zip(axes[0], rates):
        axis.plot(range(1, 13), range(1, 13), ":", color="gray", label="ideal")
        for label, data in suites.items():
            if data["particles_per_hour"] != rate:
                continue
            build = label.split(":")[0]
            rows = [r for r in data["rows"] if r["gpus"] == 0]
            axis.plot([r["threads"] for r in rows], [r["speedup"] for r in rows], "-o", color=colors[build], label=f"{build}, CPU")
        axis.set_xticks(range(1, 13))
        axis.set_xlabel("OpenMP threads, one per physical core (dimensionless)")
        axis.set_ylabel("CPU speedup T1/Tn (dimensionless)")
        axis.set_title(f"{rate:,} particles/hour")
        axis.grid(True, alpha=0.3)
        axis.legend(fontsize=8)
    figure.tight_layout()
    speedup_path = output / "cpu-speedup.svg"
    figure.savefig(speedup_path, format="svg", metadata={"Date": None})
    plt.close(figure)
    accessible(speedup_path, "Sarno CPU strong-scaling speedup on physical cores",
               "CPU speedup relative to each build's own one-thread median against 1 to 12 threads pinned to distinct "
               "physical cores, with the ideal reference. Computational diagnostic; no confidence intervals.")
    return [time_path, speedup_path]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    check = commands.add_parser("validate", help="compare one sample with the reference sample")
    check.add_argument("reference", type=Path)
    check.add_argument("candidate", type=Path)
    check.add_argument("gpus", type=int)
    collect = commands.add_parser("collect", help="summarize validated suites")
    collect.add_argument("--suite", action="append", required=True, metavar="BUILD:RATE=ROOT")
    collect.add_argument("--output", type=Path, required=True)
    collect.add_argument("--no-figures", action="store_true")
    args = parser.parse_args()
    if args.command == "validate":
        report = validate(args.reference, args.candidate, args.gpus)
        print("equivalence_passed=" + str(report["equivalence_passed"]).lower(), report.get("error", ""))
        sys.exit(0 if report["equivalence_passed"] else 3)
    try:
        suites = {}
        for item in args.suite:
            label, root = item.split("=", 1)
            suites[label] = suite(root)
        args.output.mkdir(parents=True, exist_ok=True)
        result = {"schema": "wacomm-sarno-core-sweep-v1",
                  "estimator": "median of valid measured samples; each sample sums five barrier-free solver intervals (s)",
                  "suites": suites}
        if not args.no_figures:
            result["figures"] = [p.name for p in figures(suites, args.output)]
        (args.output / "results.json").write_text(json.dumps(result, indent=2) + "\n")
        for label, data in suites.items():
            print(f"## {label} ({data['particles_per_hour']} particles/hour)")
            print("| threads | GPUs | samples | median solver (s) | range (s) | CPU speedup | CPU efficiency | GPU vs same cores | median application (s) |")
            print("| ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |")
            for r in data["rows"]:
                eff = f"{100 * r['cpu_efficiency']:.1f}%" if r["cpu_efficiency"] is not None else "–"
                gpu = f"{r['gpu_speedup_same_cores']:.2f}×" if r["gpu_speedup_same_cores"] else "–"
                print(f"| {r['threads']} | {r['gpus']} | {len(r['solver_seconds'])} | {r['solver_median']:.4f} | "
                      f"{r['solver_range'][0]:.4f}–{r['solver_range'][1]:.4f} | {r['speedup']:.2f}× | {eff} | {gpu} | "
                      f"{r['application_median']:.2f} |")
    except (ValueError, OSError, KeyError) as error:
        parser.exit(2, f"{error}\n")


if __name__ == "__main__":
    main()
