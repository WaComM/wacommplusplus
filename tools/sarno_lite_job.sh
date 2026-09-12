#!/bin/bash -l
set -euo pipefail

module load openssl/openssl-4.0.2
module load cmake/cmake-4.4.3
module load gcc-12.2.1/ompi-4.1.4_nccl
module load nvidia/cuda-12.8.0
module list > "provenance/modules-${SLURM_JOB_ID}.txt" 2>&1
uname -a > "provenance/platform-${SLURM_JOB_ID}.txt"
mpirun --version > "provenance/mpi-${SLURM_JOB_ID}.txt"
scontrol show job "$SLURM_JOB_ID" > "provenance/slurm-${SLURM_JOB_ID}.txt"
sha256sum wacommplusplus wacomm-sarno-lite-6h.json examples/sources-sarno_river.json \
    roms/rms3_d03_20210701Z*.nc > "provenance/inputs-${SLURM_JOB_ID}.sha256"
set +e
if [ "${SLURM_NTASKS:-1}" -gt 1 ]; then
    mpirun --np "$SLURM_NTASKS" --bind-to core --report-bindings \
        ./wacommplusplus wacomm-sarno-lite-6h.json
else
    ./wacommplusplus wacomm-sarno-lite-6h.json
fi
run_status=$?
printf '%s\n' "$run_status" > "provenance/exit-${SLURM_JOB_ID}.txt"
set -e
if [ "$run_status" -ne 0 ]; then exit "$run_status"; fi
find output-6h snapshots-6h processed-6h -type f -exec sha256sum {} + \
    > "provenance/outputs-${SLURM_JOB_ID}.sha256"
