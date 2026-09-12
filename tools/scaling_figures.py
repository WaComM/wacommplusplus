#!/usr/bin/env python3

import argparse
import datetime
import json
import math
import pathlib
import re
import sys

import netCDF4
import numpy as np
import xml.etree.ElementTree as ET

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.ticker import ScalarFormatter

from compare_particle_snapshots import compare
from trajectory_diagnostics import checksum


PROCESSES=[1,2,4,8,16,32]


def process_counts(root):
    if root.name=="scaling-64" and not (root/"p64").is_dir():
        raise ValueError("scaling-64 requires a 64-process run")
    return PROCESSES+[64] if (root/"p64").is_dir() else PROCESSES


def metrics(records):
    counts=[r["processes"] for r in records]
    if sorted(counts) not in (PROCESSES,PROCESSES+[64]): raise ValueError("requires 1, 2, 4, 8, 16, 32 processes, optionally with 64")
    ordered=sorted(records,key=lambda r:r["processes"])
    for record in ordered:
        if record["threads_per_process"]!=1: raise ValueError("requires one OpenMP thread per process")
        for key in ("elapsed_seconds","solver_seconds"):
            if not math.isfinite(record[key]) or record[key]<=0:
                raise ValueError("timing seconds must be finite and positive")
    baseline=ordered[0]["solver_seconds"]
    application=ordered[0]["elapsed_seconds"]
    return [dict(r,speedup=baseline/r["solver_seconds"],
                 efficiency=baseline/(r["processes"]*r["solver_seconds"]),
                 application_speedup=application/r["elapsed_seconds"],
                 application_efficiency=application/(r["processes"]*r["elapsed_seconds"])) for r in ordered]


def validate_forcing(config):
    io=config["io"]
    expected=[f"ocm3_d03_20210701Z{hour}.nc" for hour in ("09","10","11","13","14","15")]
    if io["ocean_model"]!="WaComM" or io["base_path"]!="processed-6h/" or io["save_input"] or io["nc_inputs"]!=expected:
        raise ValueError("requires read-only native forcing from processed-6h")


def solver_intervals(stdout):
    matches=re.findall(r"Solver interval: start=([^ ]+) end=([^ ]+) seconds=([^\s]+)",stdout)
    records=[{"start":float(a),"end":float(b),"seconds":float(c)} for a,b,c in matches]
    epoch=datetime.datetime(1968,5,23,tzinfo=datetime.timezone.utc)
    times=[(datetime.datetime(2021,7,1,hour,tzinfo=datetime.timezone.utc)-epoch).total_seconds() for hour in (9,10,11,13,14,15)]
    if [(r["start"],r["end"]) for r in records]!=list(zip(times[:-1],times[1:])):
        raise ValueError("requires exactly the five chronological solver intervals")
    if any(not math.isfinite(r["seconds"]) or r["seconds"]<=0 for r in records):
        raise ValueError("invalid solver interval duration")
    return records


def collect(root):
    records=[]
    baseline=root/"p1"
    validate_forcing(json.loads((baseline/"wacomm-sarno-lite-6h.json").read_text()))
    for processes in process_counts(root):
        run=root/f"p{processes}"
        provenance=run/"provenance"
        if int((provenance/"exit.txt").read_text())!=0: raise ValueError(f"{run}: failed application")
        omp=(provenance/"openmp.txt").read_text().splitlines()
        if "OMP_NUM_THREADS=1" not in omp or "OMP_THREAD_LIMIT=1" not in omp:
            raise ValueError(f"{run}: unverified OpenMP thread count")
        for name in ("wacommplusplus","wacomm-sarno-lite-6h.json","examples/sources-sarno_river/sources-sarno_river.json"):
            if checksum(run/name)!=checksum(baseline/name): raise ValueError(f"{run}: inconsistent {name}")
        stdout=(run/"run.out").read_text()
        if f"Using 1/{processes} processes, each on 1 threads." not in stdout:
            raise ValueError(f"{run}: runtime process/thread count not verified")
        if not re.search(rf"\bNumTasks={processes}\b",(provenance/"slurm.txt").read_text()):
            raise ValueError(f"{run}: Slurm task count differs")
        if not (provenance/"outputs.sha256").is_file(): raise ValueError(f"{run}: checksum collection incomplete")
        equivalence=compare(baseline/"snapshots-6h",run/"snapshots-6h")
        intervals=solver_intervals(stdout)
        record={"processes":processes,"threads_per_process":1,
                "solver_seconds":math.fsum(r["seconds"] for r in intervals),"solver_intervals":intervals,
                "elapsed_seconds":float((provenance/"elapsed-seconds.txt").read_text()),
                "job_id":int((provenance/"job-id.txt").read_text()),"application_exit_code":0,
                "particle_comparison":equivalence,
                "provenance":{p.name:p.read_text() for p in sorted(provenance.glob("*.txt"))},
                "checksums":{p.name:p.read_text() for p in sorted(provenance.glob("*.sha256"))},
                "runtime_stderr":(run/"run.err").read_text(),
                "stdout_sha256":checksum(run/"run.out")}
        records.append(record)
    return metrics(records)


def render(records,output):
    plt.rcParams.update({"font.family":"DejaVu Sans","font.size":11,"axes.titlesize":14,
                         "axes.labelsize":12,"figure.facecolor":"white","axes.facecolor":"white",
                         "savefig.facecolor":"white","svg.fonttype":"none","svg.hashsalt":"sarno-scaling"})
    counts=[r["processes"] for r in records]
    artifacts=[]
    for kind in ("speedup","efficiency"):
        fig,ax=plt.subplots(figsize=(7.2,4.8),layout="constrained")
        values=[r[kind]*(100 if kind=="efficiency" else 1) for r in records]
        application=[r["application_"+kind]*(100 if kind=="efficiency" else 1) for r in records]
        ideal=counts if kind=="speedup" else [100]*len(counts)
        ax.plot(counts,ideal,"--",color="#707070",linewidth=1.4,label="Ideal linear scaling")
        ax.plot(counts,values,"o-",color="#00689D",linewidth=2,markersize=6,label="Solver intervals (forcing excluded)")
        ax.plot(counts,application,"s-",color="#B55D12",linewidth=1.5,markersize=4,
                label="Full application (native reads + output)")
        for x,y in zip(counts,values):
            ax.annotate(f"{y:.2f}"+("%" if kind=="efficiency" else "×"),(x,y),
                        xytext=(0,10) if kind=="efficiency" else (0,-18),textcoords="offset points",ha="center",fontsize=9,bbox={"facecolor":"white","edgecolor":"none","pad":0.3,"alpha":0.9})
        ax.set_xscale("log",base=2);ax.set_xticks(counts);ax.xaxis.set_major_formatter(ScalarFormatter())
        ax.set_yscale("log",base=2)
        ax.yaxis.set_major_formatter(ScalarFormatter())
        ax.set_ylim(min(values+application)*0.55,max(ideal+values+application)*2.2)
        ax.set_xlabel("MPI processes (one OpenMP thread per process)")
        ax.set_ylabel("Speedup  T₁ / Tₚ" if kind=="speedup" else "Parallel efficiency  100 T₁ / (p Tₚ)  [%]")
        ax.set_title("Sarno lite · six-hour fixed workload",loc="left",weight="bold")
        ax.grid(True,which="major",color="#d6dce0",linewidth=0.7);ax.set_axisbelow(True)
        ax.spines[["top","right"]].set_visible(False)
        ax.legend(loc="lower left" if kind=="efficiency" else "upper left",frameon=False,fontsize=9)
        fig.get_layout_engine().set(rect=(0,0.12,1,0.88))
        layout="norm-wn · exclusive nodes · 64 ranks use two nodes" if 64 in counts else "high-wn · exclusive node"
        fig.text(0.02,0.025,f"{layout} · 1 run per point · solver baseline {records[0]['solver_seconds']:.3f} s\n"
                 "No download or ROMS preparation in any timed run. Single samples; no uncertainty bars.",fontsize=9,color="#39434d")
        for extension in ("svg","pdf","png"):
            path=output/f"sarno-{kind}.{extension}"
            fig.savefig(path,dpi=400,metadata={"Creator":"WaComM++ scaling_figures.py"})
            if extension=="svg":
                text=path.read_text();start=text.index('>',text.index('<svg'))+1
                description=f"Measured solver and full-application {kind} for {', '.join(map(str,counts))} MPI processes, one OpenMP thread each. Both axes use logarithmic scales. Single samples, no uncertainty estimates."
                text=text[:start]+f"\n<title>Sarno lite strong-scaling {kind}</title>\n<desc>{description}</desc>"+text[start:]
                path.write_text("\n".join(line.rstrip() for line in text.splitlines())+"\n")
                ET.parse(path)
            artifacts.append({"name":path.name,"sha256":checksum(path)})
        plt.close(fig)
    return artifacts


def main():
    parser=argparse.ArgumentParser(description="Verify and plot the six Sarno MPI/OpenMP strong-scaling runs")
    parser.add_argument("root",type=pathlib.Path)
    parser.add_argument("--output-dir",type=pathlib.Path,required=True)
    args=parser.parse_args()
    try:
        root=args.root.resolve();output=args.output_dir.resolve()
        if root==output or root in output.parents: raise ValueError("write reports outside the simulation root")
        records=collect(root)
        output.mkdir(parents=True,exist_ok=True)
        figures=render(records,output)
        report={"schema":"wacomm-strong-scaling-v1","records":records,"figures":figures,
                "workload":"2021-07-01 09:00–15:00 UTC, native WaComM forcing from processed-6h, dry=false, seed 5489; unchanged scientific configuration",
                "timing":{"primary":"Sum of five rank-zero steady-clock solver interval durations, bounded by MPI barriers; excludes forcing download, preparation, loading and file writes; includes source emission, scatter/gather, particle updates and concentration", "secondary":"GNU time elapsed seconds around mpirun; includes startup, native forcing reads, solver and writes; excludes downloading, ROMS preparation, staging, queue wait and checksums"},
                "estimators":{"speedup":"T1/Tp","efficiency":"T1/(p*Tp)","T":"sum of solver interval seconds; separate ratios also computed from full application elapsed seconds","time_units":"seconds","samples_per_process_count":1},
                "limitations":("Single sequential ascending sweep; 1–32 ranks use one exclusive node and 64 ranks use two exclusive nodes. " if 64 in process_counts(root) else "Single sequential ascending sweep on one exclusive node. ")+"Cache, memory bandwidth, I/O and system variation are uncontrolled. No uncertainty or multi-thread scaling estimate; efficiency uses active MPI ranks, not all reserved node cores.",
                "configuration":json.loads((root/"p1/wacomm-sarno-lite-6h.json").read_text()),
                "preparation":{"configuration":json.loads((root.parent/"preparation/wacomm-sarno-lite-6h.json").read_text()),
                               "submission_script":(root.parent/"preparation/submit.sh").read_text(),
                               "provenance":{p.name:p.read_text() for p in sorted((root.parent/"preparation/provenance").iterdir()) if p.is_file()},
                               "stdout_sha256":checksum(root.parent/"preparation/run.out"),
                               "runtime_stderr":(root.parent/"preparation/run.err").read_text()},
                "suite_provenance":{p.name:p.read_text() for p in sorted((root/"provenance").iterdir()) if p.is_file()},
                "plotting":{"python":sys.version,"numpy":np.__version__,"netCDF4":netCDF4.__version__,"matplotlib":matplotlib.__version__,"script_sha256":checksum(pathlib.Path(__file__))}}
        (output/"scaling-results.json").write_text(json.dumps(report,indent=2,sort_keys=True)+"\n")
    except (ValueError,OSError,RuntimeError) as error:
        parser.exit(2,str(error)+"\n")


if __name__=="__main__":
    main()
