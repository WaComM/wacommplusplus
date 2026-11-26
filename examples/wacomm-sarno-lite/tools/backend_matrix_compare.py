#!/usr/bin/env python3
"""Compare machines from collected backend-matrix tables as speedup factors over one reference run.

Read-only on the suites: the inputs are results.json tables written by `sarno_backend_matrix.py collect`.
For every suite the tuple with the shortest median application time is selected. Its solver and application
medians are divided into the serial-build medians of the reference machine's suite at the same release rate, which gives
dimensionless factors $X_\\mathrm{solver}=T_\\mathrm{ref}/T$ and $X_\\mathrm{app}=A_\\mathrm{ref}/A$ ("N× faster
than the reference"). Suites at a rate without a reference-machine suite, or with two, are rejected.
Usage: backend_matrix_compare.py --reference MACHINE --results MACHINE=RESULTS.json[:SUITE[,SUITE...]] ...
    --output DIR [--no-figures]
"""
import argparse
import json
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
from sarno_core_sweep import accessible  # noqa: E402

SOLVER, APPLICATION = "#2a78d6", "#eb6834"


def tuple_label(row):
    text = f"{row['family']} {row['mpi_processes']}×{row['openmp_threads']}"
    return text + (f" +{row['gpus']} GPU" if row["gpus"] else "")


def split_specification(specification):
    # The suite list follows the last colon; a colon inside a path, as in a Windows drive, is kept.
    machine, _, rest = specification.partition("=")
    path, colon, names = rest.rpartition(":")
    if not colon or not path or len(path) == 1 or "/" in names or "\\" in names:
        path, names = rest, ""
    return machine, path, names


def load(specification):
    machine, path, names = split_specification(specification)
    if not machine or not path:
        raise ValueError(f"expected MACHINE=RESULTS.json[:SUITE,...], got {specification}")
    suites = json.loads(Path(path).read_text())["suites"]
    selected = names.split(",") if names else list(suites)
    for name in selected:
        if name not in suites:
            raise ValueError(f"suite {name} not in {path}")
    return [(machine, name, suites[name]) for name in selected]


def compare(entries, reference):
    references = {}
    for machine, name, data in entries:
        if machine == reference:
            if data["particles_per_hour"] in references:
                raise ValueError(f"two {reference} suites at {data['particles_per_hour']} particles/hour")
            references[data["particles_per_hour"]] = data
    if not references:
        raise ValueError(f"reference machine {reference} not among the inputs")
    rows = []
    for machine, name, data in entries:
        rate = data["particles_per_hour"]
        if rate not in references:
            raise ValueError(f"no reference suite at {rate} particles/hour for {name}")
        base = references[rate]
        pick = min(data["rows"], key=lambda r: r["application_median"])
        rows.append({"machine": machine, "suite": name, "rate": rate, "nodes": data.get("nodes", 1),
                     "tuple": tuple_label(pick), "solver_seconds": pick["solver_median"],
                     "application_seconds": pick["application_median"],
                     "solver_factor": base["baseline_solver_seconds"] / pick["solver_median"],
                     "application_factor": base["baseline_application_seconds"] / pick["application_median"]})
    return sorted(rows, key=lambda r: (r["rate"], -r["application_factor"]))


def table(rows, reference):
    lines = [f"Speedup factors over the serial build of {reference} (fastest application tuple per suite)", ""]
    for rate in sorted({r["rate"] for r in rows}):
        lines += [f"## {rate:,} particles/hour", "",
                  "| machine | tuple | solver × | application × | solver (s) | application (s) |",
                  "| --- | --- | ---: | ---: | ---: | ---: |"]
        lines += [f"| {r['machine']} | {r['tuple']} | {r['solver_factor']:.1f}× | {r['application_factor']:.1f}× | "
                  f"{r['solver_seconds']:.3f} | {r['application_seconds']:.2f} |" for r in rows if r["rate"] == rate]
        lines.append("")
    return "\n".join(lines)


def figure(rows, reference, output):
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    from matplotlib.patches import Patch
    plt.rcParams.update({"font.size": 9, "axes.edgecolor": "#52514e", "axes.labelcolor": "#0b0b0b",
                         "xtick.color": "#52514e", "ytick.color": "#52514e", "svg.fonttype": "none",
                         "figure.facecolor": "white", "axes.facecolor": "white"})
    rates = sorted({r["rate"] for r in rows})
    heights = [sum(r["rate"] == rate for r in rows) for rate in rates]
    plot, axes = plt.subplots(len(rates), 1, figsize=(7.2, 0.62 * sum(heights) + 1.6 * len(rates)),
                              gridspec_kw={"height_ratios": heights}, squeeze=False)
    top = max(max(r["solver_factor"], r["application_factor"]) for r in rows) * 1.18
    for axis, rate in zip(axes[:, 0], rates):
        picks = [r for r in rows if r["rate"] == rate][::-1]
        for i, r in enumerate(picks):
            for offset, key, color in ((0.19, "solver_factor", SOLVER), (-0.19, "application_factor", APPLICATION)):
                axis.barh(i + offset, r[key], height=0.36, color=color, edgecolor="white", linewidth=1)
                axis.text(r[key] + top * 0.008, i + offset, f"{r[key]:.0f}×" if r[key] >= 10 else f"{r[key]:.1f}×",
                          va="center", fontsize=7.5, color="#0b0b0b")
        axis.set_yticks(range(len(picks)), [f"{r['machine']}\n{r['tuple']}" for r in picks], fontsize=7.5)
        axis.axvline(1, color="#8a8985", linewidth=1, linestyle=":")
        axis.set_xlim(0, top)
        axis.set_title(f"{rate:,} particles/hour", loc="left")
        axis.grid(True, axis="x", color="#e4e3df", linewidth=0.6)
        axis.set_axisbelow(True)
        for side in ("top", "right"):
            axis.spines[side].set_visible(False)
    axes[-1, 0].set_xlabel(f"times faster than the serial build of {reference} (median of three samples)")
    axes[0, 0].legend(handles=[Patch(color=SOLVER, label="solver intervals"),
                               Patch(color=APPLICATION, label="whole application")],
                      frameon=False, fontsize=7.5, loc="lower right")
    plot.tight_layout()
    path = output / "machine-speedup.svg"
    plot.savefig(path, format="svg", metadata={"Date": None}, bbox_inches="tight")
    plt.close(plot)
    accessible(path, "Speedup of the fastest configuration of each machine",
               f"Paired horizontal bars, one panel per release rate, of how many times faster the solver intervals "
               f"and the whole application of each machine's fastest end-to-end tuple run than the serial build of "
               f"{reference}. Measured computational diagnostic of the Sarno six-hour workload; medians of three "
               f"samples without confidence intervals; not observational validation.")
    return path


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--reference", required=True, help="machine whose serial build defines 1× at each rate")
    parser.add_argument("--results", action="append", required=True, help="MACHINE=RESULTS.json[:SUITE,...]")
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--no-figures", action="store_true")
    args = parser.parse_args()
    entries = [entry for specification in args.results for entry in load(specification)]
    rows = compare(entries, args.reference)
    args.output.mkdir(parents=True, exist_ok=True)
    (args.output / "machine-speedup.json").write_text(json.dumps(
        {"schema": "wacomm-sarno-machine-speedup-v1", "reference": args.reference, "inputs": args.results,
         "estimator": "fastest median application tuple per suite; factor = reference serial median / tuple median",
         "rows": rows}, indent=2) + "\n")
    print(table(rows, args.reference))
    if not args.no_figures:
        figure(rows, args.reference, args.output)


if __name__ == "__main__":
    main()
