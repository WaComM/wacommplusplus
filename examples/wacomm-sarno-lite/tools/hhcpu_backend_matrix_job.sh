#!/bin/bash
#SBATCH --job-name=wacomm-hh-matrix
#SBATCH --partition=hhcpu
#SBATCH --nodes=1
#SBATCH --ntasks-per-node=56
#SBATCH --cpus-per-task=1
#SBATCH --exclusive
# hhcpu defaults to 1 GiB per allocated CPU (56 GiB per node); request the whole node memory.
#SBATCH --mem=0
#SBATCH --time=12:00:00
#SBATCH --output=wacomm-hh-matrix-%j.out
# Run the Sarno backend matrix for the hhcpu CPU builds (2 x Intel Xeon Gold 5520+, 56 cores,
# 245 GB per node). Usage, from the repository root, after hhcpu_backend_test_job.sh:
#   sbatch [--nodes=N] examples/wacomm-sarno-lite/tools/hhcpu_backend_matrix_job.sh RATE SUITE CONFIG...
# CONFIG is BUILD:P:N:G as in sarno_backend_matrix.py. Every rank holds the full forcing
# (4-7 GiB), so at most 28 MPI ranks are placed on one node. With --nodes=N, MPI ranks are
# spread evenly (ppr placement); the serial and OpenMP tuples run on the first node.
set -euo pipefail

repository=${SLURM_SUBMIT_DIR:-$(cd "$(dirname "$0")/../../.." && pwd)}
cd "$repository"
module load gcc-12.2.1/ompi-4.1.4_nccl
module load python/3.13.2
python=${WACOMM_PYTHON:-$HOME/devel/wacomm-py313/bin/python}
rate=$1
suite=$2
shift 2
arguments=()
for config in "$@"; do
    arguments+=(--config "$config")
done
echo "revision=$(git rev-parse HEAD) nodes=${SLURM_JOB_NODELIST:-local} date=$(date -u +%FT%TZ)"
"$python" -I examples/wacomm-sarno-lite/tools/sarno_backend_matrix.py run --root "data/matrix-hhcpu/$suite" \
    --rate "$rate" --blocks 3 --forcing data/wacomm-sarno-lite --prune-snapshots --bind --nodes "${SLURM_JOB_NUM_NODES:-1}" --python "$python" \
    --build serial=build-hhcpu-serial/wacommplusplus --build omp=build-hhcpu-omp/wacommplusplus \
    --build mpi=build-hhcpu-mpi/wacommplusplus --build mpi-omp=build-hhcpu-mpi-omp/wacommplusplus \
    "${arguments[@]}"
