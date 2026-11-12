#!/usr/bin/env python3
"""Draw supplementary backend-matrix figures from a collected results.json.

Read-only on the suites: the input is the table written by `sarno_backend_matrix.py collect`.
Figures: CPU parallel efficiency against workers, resident forcing memory per node against MPI ranks
per node, and the solver/remaining application split of the fastest one-node and multi-node tuples.
Usage: backend_matrix_supplement.py RESULTS.json OUTPUT_DIR [--node-memory-gib X] [--default-memory-gib Y]
"""
import argparse
import json
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
from sarno_core_sweep import accessible  # noqa: E402

SERIES = (("omp", "OpenMP threads (p=1)", "#2a78d6", "o", "-"), ("mpi", "MPI ranks (n=1)", "#eb6834", "s", "-"),
          ("mpi-omp", "MPI×OpenMP hybrid", "#1baf7a", "D", "none"))


def tuple_label(row):
    return f"{row['family']} {row['mpi_processes']}×{row['openmp_threads']}"


def efficiency_points(rows, build):
    return sorted(((r["cpu_workers"], r["cpu_efficiency"], r) for r in rows
                   if r["build"] == build and r["gpus"] == 0 and r["cpu_efficiency"] is not None),
                  key=lambda x: (x[0], x[2]["mpi_processes"]))


def ranks_per_node(row, nodes):
    return max(1, row["mpi_processes"] // nodes) if "mpi" in row["build"] else 1


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("results", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--node-memory-gib", type=float, default=None, help="physical memory per node (GiB)")
    parser.add_argument("--default-memory-gib", type=float, default=None,
                        help="scheduler default memory per node without --mem (GiB)")
    args = parser.parse_args()
    suites = json.loads(args.results.read_text())["suites"]
    args.output.mkdir(parents=True, exist_ok=True)

    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    plt.rcParams.update({"font.size": 9, "axes.edgecolor": "#52514e", "axes.labelcolor": "#0b0b0b",
                         "xtick.color": "#52514e", "ytick.color": "#52514e", "svg.fonttype": "none",
                         "figure.facecolor": "white", "axes.facecolor": "white"})

    def tidy(axis):
        axis.grid(True, color="#e4e3df", linewidth=0.6)
        axis.set_axisbelow(True)
        for side in ("top", "right"):
            axis.spines[side].set_visible(False)

    def save(figure, name, title, description):
        path = args.output / name
        figure.savefig(path, format="svg", metadata={"Date": None}, bbox_inches="tight")
        plt.close(figure)
        accessible(path, title, description)
        print(path)

    # CPU parallel efficiency E = S/(p n) against workers, one panel per suite.
    figure, axes = plt.subplots(1, len(suites), figsize=(4.3 * len(suites), 3.6), squeeze=False)
    for axis, (name, data) in zip(axes[0], suites.items()):
        top = max(r["cpu_workers"] for r in data["rows"])
        axis.axhline(100, linestyle=":", color="#8a8985", linewidth=1.5, label="ideal")
        for build, text, color, marker, style in SERIES:
            points = efficiency_points(data["rows"], build)
            if not points:
                continue
            axis.plot([x for x, _, _ in points], [100 * e for _, e, _ in points], linestyle=style, marker=marker,
                      markersize=6, color=color, linewidth=2, label=text, markeredgecolor="white", markeredgewidth=1)
            if build == "mpi-omp":
                for index, (x, e, r) in enumerate(sorted(points, key=lambda q: -q[1])):
                    axis.annotate(f"{r['mpi_processes']}×{r['openmp_threads']}", (x, 100 * e), textcoords="offset points",
                                  xytext=(10, 16 - 9 * index), fontsize=6.5, color="#52514e",
                                  arrowprops={"arrowstyle": "-", "color": "#c3c2b7", "linewidth": 0.5})
        axis.set_xscale("log", base=2)
        axis.set_xlim(0.8, top * 3)
        axis.set_ylim(0, 110)
        axis.set_xlabel("CPU workers p·n (dimensionless, log₂ scale)")
        axis.set_ylabel("CPU parallel efficiency E (%)")
        axis.set_title(f"{name}, {data['particles_per_hour']:,} p/h, {data.get('nodes', 1)} node(s)", loc="left",
                       fontsize=8.5)
        tidy(axis)
        axis.legend(fontsize=7, frameon=False, loc="lower left")
    figure.tight_layout()
    save(figure, "cpu-efficiency.svg", "CPU parallel efficiency by decomposition",
         "Line and marker chart of CPU parallel efficiency, solver speedup over the serial build divided by MPI ranks "
         "times OpenMP threads, in percent, against CPU workers on a base-two logarithmic axis, one panel per suite, for "
         "the OpenMP thread sweep, the MPI rank sweep, and labelled hybrid placements, with the ideal 100 percent line. "
         "Computational diagnostic of measured execution; no confidence intervals.")

    # Resident set per rank times ranks per node, against ranks per node.
    figure, axis = plt.subplots(figsize=(6.4, 3.8))
    markers = ("o", "s", "D", "^")
    colors = ("#2a78d6", "#eb6834", "#1baf7a", "#8a5cc2")
    top = 1
    for index, (name, data) in enumerate(suites.items()):
        nodes = data.get("nodes", 1)
        points = sorted({(ranks_per_node(r, nodes), ranks_per_node(r, nodes) * r["max_rss_kib_median"] / 2**20)
                         for r in data["rows"] if "mpi" in r["build"]})
        top = max([top] + [k for k, _ in points])
        axis.plot([k for k, _ in points], [m for _, m in points], marker=markers[index % 4], color=colors[index % 4],
                  linewidth=1.5, markersize=5, label=f"{name} ({data['particles_per_hour']:,} p/h)",
                  markeredgecolor="white", markeredgewidth=0.8)
    if args.node_memory_gib:
        axis.axhline(args.node_memory_gib, color="#0b0b0b", linewidth=1)
        axis.text(1.05, args.node_memory_gib * 1.04, f"node memory {args.node_memory_gib:.0f} GiB", fontsize=7.5)
    if args.default_memory_gib:
        axis.axhline(args.default_memory_gib, color="#c0392b", linewidth=1, linestyle="--")
        axis.text(1.05, args.default_memory_gib * 1.04, f"scheduler default {args.default_memory_gib:.0f} GiB",
                  fontsize=7.5, color="#c0392b")
    axis.set_xlabel("MPI ranks per node (dimensionless)")
    axis.set_ylabel("ranks × median peak RSS per rank (GiB)")
    axis.set_xlim(0.8, top * 1.1)
    axis.set_ylim(bottom=0)
    tidy(axis)
    axis.legend(fontsize=7.5, frameon=False, loc="upper left", bbox_to_anchor=(0, 0.92))
    save(figure, "memory-per-node.svg", "Replicated forcing memory per node",
         "Line chart of the product of MPI ranks per node and the median peak resident memory of one rank, in GiB, "
         "against MPI ranks per node, one line per suite, with horizontal lines for physical node memory and the "
         "scheduler's default job memory. Each rank holds the full forcing, so the product is an estimate of node "
         "demand, not a measured node total. Computational diagnostic; no confidence intervals.")

    # Solver and remaining application time of the fastest tuples per suite.
    picks = []
    for name, data in suites.items():
        for build in ("omp", "mpi", "mpi-omp"):
            rows = [r for r in data["rows"] if r["build"] == build]
            if rows:
                picks.append((name, data.get("nodes", 1), min(rows, key=lambda r: r["solver_median"])))
    picks.sort(key=lambda x: x[2]["application_median"], reverse=True)
    figure, axis = plt.subplots(figsize=(7.0, 0.32 * len(picks) + 1.3))
    y = range(len(picks))
    solver = [r["solver_median"] for _, _, r in picks]
    rest = [max(r["application_median"] - r["solver_median"], 0) for _, _, r in picks]
    axis.barh(y, solver, height=0.7, color="#2a78d6", edgecolor="white", linewidth=1, label="solver intervals")
    axis.barh(y, rest, left=solver, height=0.7, color="#c3c2b7", edgecolor="white", linewidth=1,
              label="start-up, forcing input, output")
    for i, (_, _, r) in zip(y, picks):
        axis.text(r["application_median"] + 0.4, i, f"{r['solver_median']:.2f} + {r['application_median'] - r['solver_median']:.1f} s",
                  va="center", fontsize=7, color="#52514e")
    axis.set_yticks(list(y), [f"{tuple_label(r)}, {name}" for name, _, r in picks], fontsize=7)
    axis.set_xlabel("median application wall time (s)")
    axis.set_xlim(right=max(r["application_median"] for _, _, r in picks) * 1.3)
    tidy(axis)
    axis.grid(False, axis="y")
    axis.legend(fontsize=7.5, frameon=False, loc="lower right")
    save(figure, "application-split.svg", "Solver and non-solver time of the fastest tuples",
         "Stacked horizontal bars of median solver time and remaining application wall time, in seconds, for the "
         "fastest solver tuple of the OpenMP, MPI, and hybrid builds in every suite, labelled with both parts. The "
         "remainder includes start-up, forcing input from the shared file system, and output. Computational "
         "diagnostic; no confidence intervals.")


if __name__ == "__main__":
    main()
