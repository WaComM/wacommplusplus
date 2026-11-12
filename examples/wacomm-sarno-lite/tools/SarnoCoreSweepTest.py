#!/usr/bin/env python3
import json
import shutil
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from sarno_core_sweep import INTERVALS, suite

TOPOLOGY = "CPU CORE\n" + "".join(f"{cpu} {cpu % 4}\n" for cpu in range(8))


def sample(root, threads, gpus, label, seconds, cpus=None, reference=None):
    directory = root / f"p1_n{threads}_g{gpus}" / f"sample-{label}"
    directory.mkdir(parents=True)
    lines = [f"Using 1/1 processes, each on {threads} threads.", f"Acceleration: CUDA {gpus} device(s)",
             "Emitted particles: 10 Total particles: 10"]
    lines += [f"Solver interval: start={a:.17g} end={b:.17g} seconds={seconds / 5}" for a, b in zip(INTERVALS[:-1], INTERVALS[1:])]
    if gpus:
        lines += ["CUDA HOST2DEV sec: 0.1", "GPU Processed in sec: 0.01"]
    lines.append("EVALUATE sec: 0.02")
    (directory / "stdout.txt").write_text("\n".join(lines) + "\n")
    cpus = list(range(threads)) if cpus is None else cpus
    (directory / "stderr.txt").write_text("".join(f"level 1 thread 0x{index:x} affinity {cpu}\n" for index, cpu in enumerate(cpus) if threads > 1))
    (directory / "time.txt").write_text(f"elapsed_seconds={seconds + 3} maxrss_kb=1\n")
    (directory / "exit.txt").write_text("0\n")
    report = {"equivalence_passed": True, "reference": str(reference or root / "p1_n1_g0" / "sample-warmup")}
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
        (root / "provenance/cpu-topology.txt").write_text(TOPOLOGY)
        (root / "provenance/particles-per-hour.txt").write_text("10000\n")
        (root / "provenance/binary-revision.txt").write_text("abc\n")
        (root / "provenance/binary-working-tree.patch").write_text("")
        for label, factor in (("1", 1.0), ("2", 1.1), ("3", 0.9)):
            sample(root, 1, 0, label, 8 * factor)
            sample(root, 4, 0, label, 2.5 * factor)
            sample(root, 4, 1, label, 1 * factor)
        result = suite(root)
        rows = {(r["threads"], r["gpus"]): r for r in result["rows"]}
        assert abs(result["baseline_solver_seconds"] - 8) < 1e-12
        assert abs(rows[4, 0]["speedup"] - 3.2) < 1e-12 and abs(rows[4, 0]["cpu_efficiency"] - 0.8) < 1e-12
        assert abs(rows[4, 1]["gpu_speedup_same_cores"] - 2.5) < 1e-12 and rows[4, 1]["cpu_efficiency"] is None
        assert abs(rows[4, 1]["components_median"]["host_to_device_seconds"] - 0.1) < 1e-12
        assert all(abs(a - b) < 1e-12 for a, b in zip(rows[4, 1]["solver_range"], (0.9, 1.1)))
        # Logical CPUs 0 and 4 are the same physical core in this topology.
        sample(root, 2, 0, "1", 4, cpus=[0, 4])
        assert rejected(lambda: suite(root)), "shared physical core accepted"
        shutil.rmtree(root / "p1_n2_g0")
        sample(root, 2, 0, "2", 4, reference=root / "elsewhere")
        assert rejected(lambda: suite(root)), "wrong reference accepted"
        shutil.rmtree(root / "p1_n2_g0")
        sample(root, 2, 0, "3", 4)
        text = (root / "p1_n2_g0/sample-3/stdout.txt").read_text().replace("on 2 threads", "on 3 threads")
        (root / "p1_n2_g0/sample-3/stdout.txt").write_text(text)
        assert rejected(lambda: suite(root)), "wrong thread count accepted"


if __name__ == "__main__":
    main()
