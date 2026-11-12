#!/usr/bin/env python3
import json
import shutil
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from sarno_backend_matrix import INTERVALS, best, launch_command, parse_config, suite


def sample(root, build, p, n, g, label, seconds, application=None, reference=None, runtime=None):
    directory = root / f"{build}_p{p}_n{n}_g{g}" / f"sample-{label}"
    directory.mkdir(parents=True)
    lines = [f"0: Using 1/{runtime or p} processes, each on {n} threads.", "Emitted particles: 10 Total particles: 10"]
    if "cuda" in build:
        lines.append(f"Acceleration: CUDA {g} device(s)")
    lines += [f"Solver interval: start={a:.17g} end={b:.17g} seconds={seconds / 5}" for a, b in zip(INTERVALS[:-1], INTERVALS[1:])]
    lines.append("EVALUATE sec: 0.02")
    (directory / "stdout.txt").write_text("\n".join(lines) + "\n")
    (directory / "time.txt").write_text(f"elapsed_seconds={application or seconds + 3} maxrss_kb=1048576\n")
    (directory / "exit.txt").write_text("0\n")
    report = {"equivalence_passed": True, "reference": str(reference or root / "serial_p1_n1_g0" / "sample-warmup")}
    (directory / "validation.json").write_text(json.dumps(report))


def rejected(function):
    try:
        function()
    except ValueError:
        return True
    return False


def main():
    with tempfile.TemporaryDirectory() as temporary:
        root = Path(temporary) / "suite"
        (root / "provenance").mkdir(parents=True)
        (root / "provenance/particles-per-hour.txt").write_text("100000\n")
        (root / "provenance/suite.json").write_text(json.dumps({"host": "h", "platform": "p", "revision": "r",
                                                                  "working_tree_patch": ""}))
        (root / "serial_p1_n1_g0/sample-warmup").mkdir(parents=True)
        for label, factor in (("1", 1.0), ("2", 1.1), ("3", 0.9)):
            sample(root, "serial", 1, 1, 0, label, 8 * factor)
            sample(root, "mpi-omp", 2, 2, 0, label, 2.5 * factor)
            sample(root, "omp-cuda", 1, 4, 1, label, 1 * factor, application=20)
        result = suite(root)
        rows = {(r["build"], r["mpi_processes"], r["openmp_threads"], r["gpus"]): r for r in result["rows"]}
        hybrid = rows["mpi-omp", 2, 2, 0]
        assert abs(hybrid["speedup"] - 3.2) < 1e-12 and abs(hybrid["cpu_efficiency"] - 0.8) < 1e-12
        assert hybrid["cpu_workers"] == 4 and abs(hybrid["max_rss_kib_median"] - 1048576) < 1e-9
        gpu = rows["omp-cuda", 1, 4, 1]
        assert gpu["cpu_efficiency"] is None and abs(gpu["speedup"] - 8) < 1e-12
        # The GPU tuple has the fastest solver but the hybrid CPU tuple has the fastest application.
        assert best(result["rows"], "solver_median") is gpu
        assert best(result["rows"], "solver_median", gpus=False) is hybrid
        assert best(result["rows"], "application_median") is hybrid
        assert [r["build"] for r in result["rows"]] == ["serial", "mpi-omp", "omp-cuda"]
        # Resource tuples a build cannot provide are refused before anything runs.
        for text in ("omp:2:1:0", "mpi:1:2:0", "omp:1:2:1", "unknown:1:1:0", "serial:0:1:0"):
            assert rejected(lambda: parse_config(text)), text
        assert parse_config("mpi-omp-cuda:2:6:1") == ("mpi-omp-cuda", 2, 6, 1)
        assert launch_command("omp", 1, 4, "w", True) == ["w", "wacomm-sarno-lite-6h.json"]
        bound = launch_command("mpi-omp", 2, 6, "w", True)
        assert bound[:7] == ["mpirun", "-np", "2", "--map-by", "slot:PE=6", "--bind-to", "core"], bound
        assert launch_command("mpi", 2, 1, "w", False)[3:5] == ["--bind-to", "none"]
        spread = launch_command("mpi-omp", 8, 28, "w", True, nodes=4)
        assert spread[3:7] == ["--map-by", "ppr:2:node:PE=28", "--bind-to", "core"], spread
        assert launch_command("mpi", 4, 1, "w", False, nodes=2)[3:7] == ["--map-by", "ppr:2:node", "--bind-to", "none"]
        try:
            launch_command("mpi", 3, 1, "w", True, nodes=2)
            raise AssertionError("uneven rank placement accepted")
        except ValueError:
            pass
        # Wrong runtime rank count, wrong reference, and a missing CUDA line all invalidate the only sample.
        sample(root, "mpi", 2, 1, 0, "1", 4, runtime=3)
        assert rejected(lambda: suite(root)), "wrong rank count accepted"
        shutil.rmtree(root / "mpi_p2_n1_g0")
        sample(root, "mpi", 2, 1, 0, "1", 4, reference=root / "elsewhere")
        assert rejected(lambda: suite(root)), "wrong reference accepted"
        shutil.rmtree(root / "mpi_p2_n1_g0")
        sample(root, "cuda", 1, 1, 1, "1", 4)
        text = (root / "cuda_p1_n1_g1/sample-1/stdout.txt").read_text().replace("CUDA 1 device", "CUDA 0 device")
        (root / "cuda_p1_n1_g1/sample-1/stdout.txt").write_text(text)
        assert rejected(lambda: suite(root)), "missing GPU accepted"


if __name__ == "__main__":
    main()
