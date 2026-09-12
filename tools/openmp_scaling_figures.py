#!/usr/bin/env python3

import argparse
import json
import math
import pathlib
import re
import sys

import netCDF4
import numpy as np

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.ticker import ScalarFormatter

from scaling_figures import solver_intervals, validate_forcing
from compare_particle_snapshots import compare
from trajectory_diagnostics import checksum


THREADS=[1,2,4,8,16,32]


def metrics(records):
    if sorted(r["threads_per_process"] for r in records)!=THREADS:
        raise ValueError("requires exactly 1, 2, 4, 8, 16, 32 threads")
    ordered=sorted(records,key=lambda r:r["threads_per_process"])
    for r in ordered:
        if r["processes"]!=1: raise ValueError("requires exactly one MPI process")
        for key in ("solver_seconds","elapsed_seconds"):
            if not math.isfinite(r[key]) or r[key]<=0: raise ValueError("invalid timing")
    return [dict(r,speedup=ordered[0]["solver_seconds"]/r["solver_seconds"],
                 efficiency=ordered[0]["solver_seconds"]/(r["threads_per_process"]*r["solver_seconds"]),
                 application_speedup=ordered[0]["elapsed_seconds"]/r["elapsed_seconds"],
                 application_efficiency=ordered[0]["elapsed_seconds"]/(r["threads_per_process"]*r["elapsed_seconds"])) for r in ordered]


def affinity(stderr,threads):
    pairs=re.findall(r"^level 1 thread (\S+) affinity (\d+)$",stderr,re.MULTILINE)
    if threads==1 and not pairs:
        place=re.search(r"OMP_PLACES = '\{(\d+)\}'",stderr)
        bindings=re.findall(r"core (\d+)\[hwt (\d+)\]",stderr)
        if place and len(bindings)==1: return [int(place.group(1))]
    workers={worker for worker,cpu in pairs};cpus=sorted({int(cpu) for worker,cpu in pairs})
    if len(workers)!=threads or len(cpus)!=threads:
        raise ValueError("requires one distinct physical-core affinity per OpenMP worker")
    return cpus


def collect(root,mpi_root):
    records=[]
    reference=mpi_root/"p1"
    validate_forcing(json.loads((reference/"wacomm-sarno-lite-6h.json").read_text()))
    for threads in THREADS:
        run=root/f"t{threads}";provenance=run/"provenance"
        if int((provenance/"exit.txt").read_text())!=0: raise ValueError(f"{run}: failed application")
        for name in ("wacommplusplus","wacomm-sarno-lite-6h.json","examples/sources-sarno_river.json"):
            if checksum(run/name)!=checksum(reference/name): raise ValueError(f"{run}: differs from MPI reference: {name}")
        omp=(provenance/"openmp.txt").read_text().splitlines()
        for expected in (f"OMP_NUM_THREADS={threads}",f"OMP_THREAD_LIMIT={threads}","OMP_DYNAMIC=FALSE","OMP_PROC_BIND=SPREAD","OMP_PLACES=cores"):
            if expected not in omp: raise ValueError(f"{run}: missing {expected}")
        if not re.search(r"Thread\(s\) per core:\s+1\b",(provenance/"cpu.txt").read_text()):
            raise ValueError("physical-core comparison requires one hardware thread per core")
        slurm=(provenance/"slurm.txt").read_text()
        if not re.search(r"\bNumTasks=1\b",slurm) or not re.search(rf"\bCPUs/Task={threads}\b",slurm):
            raise ValueError(f"{run}: inconsistent Slurm allocation")
        stdout=(run/"run.out").read_text()
        if f"Using 1/1 processes, each on {threads} threads." not in stdout:
            raise ValueError(f"{run}: runtime thread count differs")
        if not (provenance/"outputs.sha256").is_file(): raise ValueError(f"{run}: checksum collection incomplete")
        intervals=solver_intervals(stdout)
        cpus=affinity((run/"run.err").read_text(),threads)
        records.append({"processes":1,"threads_per_process":threads,
                        "job_id":int((provenance/"job-id.txt").read_text()),"application_exit_code":0,"affinity_cpus":cpus,
                        "solver_seconds":math.fsum(r["seconds"] for r in intervals),"solver_intervals":intervals,
                        "elapsed_seconds":float((provenance/"elapsed-seconds.txt").read_text()),
                        "mpi_particle_comparison":compare(reference/"snapshots-6h",run/"snapshots-6h"),
                        "provenance":{p.name:p.read_text() for p in sorted(provenance.iterdir()) if p.is_file()},
                        "runtime_stderr":(run/"run.err").read_text(),"stdout_sha256":checksum(run/"run.out")})
    return metrics(records)


def render(records,mpi_records,output):
    plt.rcParams.update({"font.family":"DejaVu Sans","font.size":10,"axes.labelsize":11,
                         "axes.titlesize":13,"figure.facecolor":"white","axes.facecolor":"white",
                         "savefig.facecolor":"white","svg.fonttype":"none","svg.hashsalt":"sarno-openmp"})
    artifacts=[]
    shared=[r for r in mpi_records if r["processes"] in THREADS]
    for kind in ("speedup","efficiency"):
        factor=100 if kind=="efficiency" else 1
        values=[r[kind]*factor for r in records]
        application=[r["application_"+kind]*factor for r in records]
        mpi=[r[kind]*factor for r in shared]
        ideal=THREADS if kind=="speedup" else [100]*len(THREADS)
        fig,ax=plt.subplots(figsize=(7.6,5.2),layout="constrained")
        ax.plot(THREADS,ideal,"--",color="#777777",linewidth=1.2,label="Ideal linear scaling")
        ax.plot(THREADS,values,"o-",color="#00689D",linewidth=2,label="OpenMP solver (forcing excluded)")
        ax.plot(THREADS,application,"s-",color="#B55D12",linewidth=1.5,label="OpenMP full application")
        ax.plot([r["processes"] for r in shared],mpi,"^--",color="#39764B",linewidth=1.5,
                label="MPI solver · matching core counts only")
        for x,y in zip(THREADS,values):
            ax.annotate(f"{y:.2f}"+("%" if kind=="efficiency" else "×"),(x,y),
                        xytext=(0,10) if kind=="efficiency" else (0,-18),textcoords="offset points",
                        ha="center",fontsize=8,bbox={"facecolor":"white","edgecolor":"none","pad":0.3,"alpha":0.9})
        ax.set_xscale("log",base=2);ax.set_xticks(THREADS);ax.xaxis.set_major_formatter(ScalarFormatter())
        ax.set_yscale("log",base=2);ax.yaxis.set_major_formatter(ScalarFormatter())
        ax.set_ylim(min(values+application+mpi)*0.45,max(ideal+values+application+mpi)*2.5)
        ax.set_xlabel("Active cores: OpenMP threads / MPI processes")
        ax.set_ylabel("Speedup  T₁ / Tₙ" if kind=="speedup" else "Efficiency  100 T₁ / (n Tₙ)  [%]")
        ax.set_title("Sarno lite · one MPI process, varying OpenMP threads",loc="left",weight="bold")
        ax.grid(True,color="#d6dce0",linewidth=0.7);ax.set_axisbelow(True)
        ax.spines[["top","right"]].set_visible(False)
        ax.legend(loc="lower left" if kind=="efficiency" else "upper left",frameon=False,fontsize=8)
        fig.get_layout_engine().set(rect=(0,0.14,1,0.86))
        fig.text(.02,.025,f"Exclusive node · same executable, native forcing and six-hour workload\n"
                 f"Separate solver baselines: OpenMP {records[0]['solver_seconds']:.3f} s; MPI {mpi_records[0]['solver_seconds']:.3f} s\n"
                 "One observation per point; no uncertainty bars. MPI and OpenMP use the same active core counts.",fontsize=8,color="#39434d")
        for extension in ("svg","pdf","png"):
            path=output/f"sarno-openmp-{kind}.{extension}"
            fig.savefig(path,dpi=400,metadata={"Creator":"WaComM++ openmp_scaling_figures.py"})
            if extension=="svg":
                text=path.read_text();start=text.index('>',text.index('<svg'))+1
                text=text[:start]+f"\n<title>Sarno OpenMP {kind} and MPI comparison</title>\n<desc>Measured OpenMP solver and application {kind} at 1, 2, 4, 8, 16, 32 threads with one MPI process; historical MPI solver reference at matching core counts through 32. Logarithmic axes. Each sweep uses its own one-core baseline; single observations, no confidence intervals.</desc>"+text[start:]
                path.write_text("\n".join(line.rstrip() for line in text.splitlines())+"\n")
            artifacts.append({"name":path.name,"sha256":checksum(path)})
        plt.close(fig)
    return artifacts


def main():
    parser=argparse.ArgumentParser(description="Verify the one-process OpenMP sweep and compare it with MPI")
    parser.add_argument("root",type=pathlib.Path)
    parser.add_argument("--mpi-root",type=pathlib.Path,required=True)
    parser.add_argument("--mpi-report",type=pathlib.Path,required=True)
    parser.add_argument("--output-dir",type=pathlib.Path,required=True)
    args=parser.parse_args()
    try:
        root=args.root.resolve();output=args.output_dir.resolve();mpi_root=args.mpi_root.resolve()
        if any(p==output or p in output.parents for p in (root,mpi_root)):
            raise ValueError("output must be outside both simulation roots")
        records=collect(root,mpi_root)
        mpi=json.loads(args.mpi_report.read_text())
        for r in mpi["records"]:
            if checksum(mpi_root/f"p{r['processes']}"/"run.out")!=r["stdout_sha256"]:
                raise ValueError("MPI report does not match archived logs")
        if (root/"provenance/forcing.sha256").read_text()!=(mpi_root/"provenance/forcing.sha256").read_text():
            raise ValueError("forcing manifests differ")
        output.mkdir(parents=True,exist_ok=True)
        artifacts=render(records,mpi["records"],output)
        report={"schema":"wacomm-openmp-scaling-v1","records":records,"figures":artifacts,
                "configuration":json.loads((root/"t1/wacomm-sarno-lite-6h.json").read_text()),
                "mpi_reference":{"report_sha256":checksum(args.mpi_report),"records":mpi["records"]},
                "estimators":"Each sweep uses its own one-core baseline. Solver speedup T1/Tn and efficiency T1/(n*Tn); application ratios use independent application baselines. Times in seconds; efficiency dimensionless.",
                "timing":mpi["timing"],
                "limitations":"Single ascending sweep; historical MPI comparison, no repeated/randomized timings. Cache, frequency, NUMA placement and system state are uncontrolled; no confidence intervals or causal bottleneck attribution.",
                "suite_provenance":{p.name:p.read_text() for p in sorted((root/"provenance").iterdir()) if p.is_file()},
                "plotting":{"python":sys.version,"numpy":np.__version__,"netCDF4":netCDF4.__version__,"matplotlib":matplotlib.__version__,
                            "scripts":{name:checksum(pathlib.Path(__file__).parent/name) for name in ("openmp_scaling_figures.py","scaling_figures.py","compare_particle_snapshots.py","trajectory_diagnostics.py")}}}
        (output/"openmp-scaling-results.json").write_text(json.dumps(report,indent=2,sort_keys=True)+"\n")
    except (ValueError,OSError,RuntimeError) as error:
        parser.exit(2,str(error)+"\n")


if __name__=="__main__":main()
