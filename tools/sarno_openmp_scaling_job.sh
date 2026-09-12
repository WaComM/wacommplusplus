#!/bin/bash -l
set -euo pipefail

module load openssl/openssl-4.0.2
module load cmake/cmake-4.4.3
module load gcc-12.2.1/ompi-4.1.4_nccl
module load nvidia/cuda-12.8.0
export OMP_NUM_THREADS="$SLURM_CPUS_PER_TASK"
export OMP_THREAD_LIMIT="$SLURM_CPUS_PER_TASK"
export OMP_DYNAMIC=FALSE
export OMP_PROC_BIND=SPREAD
export OMP_DISPLAY_ENV=VERBOSE
export OMP_DISPLAY_AFFINITY=TRUE
export OMP_PLACES=cores
module list > provenance/modules.txt 2>&1
uname -a > provenance/platform.txt
lscpu > provenance/cpu.txt
mpirun --version > provenance/mpi.txt
/usr/bin/time --version > provenance/timer.txt
scontrol show job "$SLURM_JOB_ID" > provenance/slurm.txt
printenv | LC_ALL=C sort | sed -n '/^OMP_/p' > provenance/openmp.txt
set +e
/usr/bin/time -f '%e' -o provenance/elapsed-seconds.txt \
    mpirun --np 1 --map-by "slot:PE=$SLURM_CPUS_PER_TASK" --bind-to core --report-bindings \
    ./wacommplusplus wacomm-sarno-lite-6h.json
status=$?
printf '%s\n' "$status" > provenance/exit.txt
set -e
if [ "$status" -ne 0 ]; then exit "$status"; fi
find output-6h snapshots-6h -type f -exec sha256sum {} + \
    > provenance/outputs.sha256
