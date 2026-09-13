#!/usr/bin/env python3

import argparse
import json
import math
import pathlib
import re
import sys

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.ticker import ScalarFormatter

from compare_particle_snapshots import compare
from scaling_figures import solver_intervals, validate_forcing
from trajectory_diagnostics import checksum


COUNTS=(1,2,4)


def collect(root):
    baseline=root/"p1"
    config=json.loads((baseline/"wacomm-sarno-lite-6h.json").read_text())
    validate_forcing(config)
    records=[]
    for p in COUNTS:
        run=root/f"p{p}"
        provenance=run/"provenance"
        if int((provenance/"exit.txt").read_text())!=0: raise ValueError(f"{run}: failed application")
        for name in ("wacommplusplus","wacomm-sarno-lite-6h.json","examples/sources-sarno_river/sources-sarno_river.json"):
            if checksum(run/name)!=checksum(baseline/name): raise ValueError(f"{run}: inconsistent {name}")
        stdout=(run/"run.out").read_text()
        if f"Using 1/{p} processes, each on 1 threads." not in stdout: raise ValueError(f"{run}: rank/thread count differs")
        bindings=re.findall(r"GPU binding: rank=(\d+) host=([^ ]+) device=(\d+)",stdout)
        if len(bindings)!=p or sorted(int(rank) for rank,_,_ in bindings)!=list(range(p)):
            raise ValueError(f"{run}: incomplete GPU bindings")
        if sorted((host,int(device)) for _,host,device in bindings)!=sorted(set((host,int(device)) for _,host,device in bindings)):
            raise ValueError(f"{run}: GPU shared between ranks")
        if len(set(host for _,host,_ in bindings))!=(p+3)//4: raise ValueError(f"{run}: node count differs")
        if stdout.count("Acceleration: CUDA 1 device(s)")!=1: raise ValueError(f"{run}: rank-zero CUDA device count differs")
        omp=(provenance/"openmp.txt").read_text().splitlines()
        if "OMP_NUM_THREADS=1" not in omp or "OMP_THREAD_LIMIT=1" not in omp: raise ValueError(f"{run}: OpenMP count differs")
        if not re.search(rf"\bNumTasks={p}\b",(provenance/"slurm.txt").read_text()): raise ValueError(f"{run}: allocation differs")
        if not (provenance/"outputs.sha256").is_file(): raise ValueError(f"{run}: missing output hashes")
        intervals=solver_intervals(stdout)
        record={"processes":p,"nodes":(p+3)//4,"gpus_per_process":1,"threads_per_process":1,
                "job_id":int((provenance/"job-id.txt").read_text()),
                "solver_intervals":intervals,"solver_seconds":math.fsum(x["seconds"] for x in intervals),
                "elapsed_seconds":float((provenance/"elapsed-seconds.txt").read_text()),
                "gpu_bindings":bindings,"particle_comparison":compare(baseline/"snapshots-6h",run/"snapshots-6h"),
                "provenance":{q.name:q.read_text() for q in provenance.iterdir() if q.is_file() and q.suffix==".txt"},
                "checksums":{q.name:q.read_text() for q in provenance.iterdir() if q.is_file() and q.suffix==".sha256"},
                "stdout_sha256":checksum(run/"run.out"),"runtime_stderr":(run/"run.err").read_text()}
        for key in ("solver_seconds","elapsed_seconds"):
            if not math.isfinite(record[key]) or record[key]<=0: raise ValueError(f"{run}: invalid timing")
        records.append(record)
    for record in records:
        p=record["processes"]
        for key,source in (("solver","solver_seconds"),("application","elapsed_seconds")):
            speedup=records[0][source]/record[source]
            record[key+"_speedup"]=speedup
            record[key+"_efficiency"]=speedup/p
    return config,records


def render(records,output):
    plt.rcParams.update({"font.family":"DejaVu Sans","font.size":11,"figure.facecolor":"white",
                         "axes.facecolor":"white","savefig.facecolor":"white","svg.fonttype":"none"})
    counts=list(COUNTS)
    artifacts=[]
    for kind in ("speedup","efficiency"):
        fig,ax=plt.subplots(figsize=(7.2,4.8),layout="constrained")
        scale=100 if kind=="efficiency" else 1
        ax.plot(counts,[p if kind=="speedup" else 100 for p in counts],"--",color="#707070",label="Ideal linear scaling")
        ax.plot(counts,[r["solver_"+kind]*scale for r in records],"o-",color="#00689D",label="CUDA solver intervals")
        ax.plot(counts,[r["application_"+kind]*scale for r in records],"s-",color="#B55D12",label="Full application")
        ax.set_xscale("log",base=2);ax.set_xticks(counts);ax.xaxis.set_major_formatter(ScalarFormatter())
        ax.set_yscale("log",base=2);ax.yaxis.set_major_formatter(ScalarFormatter())
        ax.set_xlabel("MPI ranks and active GPUs (one OpenMP thread per rank)")
        ax.set_ylabel("Speedup  T₁ / Tₚ" if kind=="speedup" else "Parallel efficiency  100 T₁ / (p Tₚ)  [%]")
        ax.set_title("Sarno lite · MPI + OpenMP + CUDA",loc="left",weight="bold")
        ax.grid(True,which="major",color="#d6dce0");ax.set_axisbelow(True)
        ax.spines[["top","right"]].set_visible(False);ax.legend(frameon=False,fontsize=9)
        fig.get_layout_engine().set(rect=(0,0.12,1,0.88))
        fig.text(0.02,0.025,"low-gn · one exclusive node · one run per point\n"
                 "Native forcing; preparation excluded. GPU/CPU trajectories differ; see example discussion.",fontsize=9,color="#39434d")
        for extension in ("svg","pdf","png"):
            path=output/f"sarno-cuda-{kind}.{extension}"
            fig.savefig(path,dpi=400,metadata={"Creator":"WaComM++ cuda_scaling_figures.py"})
            if extension=="svg":
                content=path.read_text();index=content.index(">",content.index("<svg"))+1
                content=content[:index]+f"\n<title>Sarno CUDA {kind}</title>\n<desc>Measured CUDA solver and full-application {kind} at 1, 2, and 4 MPI ranks with one GPU per rank on one node. Single samples without uncertainty estimates.</desc>"+content[index:]
                path.write_text("\n".join(line.rstrip() for line in content.splitlines())+"\n")
            artifacts.append({"name":path.name,"sha256":checksum(path)})
        plt.close(fig)
    return artifacts


def main():
    parser=argparse.ArgumentParser(description="Verify and plot Sarno CUDA scaling runs")
    parser.add_argument("root",type=pathlib.Path)
    parser.add_argument("--output-dir",type=pathlib.Path,required=True)
    args=parser.parse_args()
    root=args.root.resolve();output=args.output_dir.resolve()
    if output==root or root in output.parents: parser.exit(2,"report must be outside simulation root\n")
    try:
        config,records=collect(root)
        output.mkdir(parents=True,exist_ok=True)
        figures=render(records,output)
        report={"schema":"wacomm-cuda-scaling-v1","configuration":config,"records":records,"figures":figures,
                "backend_equivalence":{"cpu_reference":"scaling-64/p1","equal":False,
                    "first_difference":"2021-07-01 10:00 UTC: depth values differ under zero-tolerance particle comparison",
                    "max_absolute_difference_at_10":{"longitude_degrees":1.273975236415481e-8,
                        "latitude_degrees":8.359890557585459e-10,"depth_metres":9.534231152339735e-8},
                    "active_particles_at_14":{"cpu":32393,"cuda":32394},
                    "cuda_particle_parity_test":"failed position assertion on Tesla V100"},
                "timing":{"solver":"sum of five barrier-bounded physical-interval seconds; excludes forcing reads and writes",
                          "application":"GNU time around mpirun; includes native reads and output writes, excludes preparation and queue wait"},
                "estimators":{"speedup":"T1/Tp","efficiency":"T1/(p*Tp)","samples_per_count":1},
                "limitations":"One ascending sweep on one exclusive low-gn node. GPU/CPU particle states differ, so this is a computational diagnostic rather than backend-validated scientific performance. No timing uncertainty estimate.",
                "suite_provenance":{q.name:q.read_text() for q in (root/"provenance").iterdir() if q.is_file()},
                "plotting":{"python":sys.version,"matplotlib":matplotlib.__version__,"script_sha256":checksum(pathlib.Path(__file__))}}
        (output/"cuda-scaling-results.json").write_text(json.dumps(report,indent=2,sort_keys=True)+"\n")
    except (ValueError,OSError,RuntimeError) as error:
        parser.exit(2,str(error)+"\n")


if __name__=="__main__": main()
