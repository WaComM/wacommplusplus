#!/bin/bash
#SBATCH --job-name=wacomm-hh-test
#SBATCH --partition=hhcpu
#SBATCH --nodes=1
#SBATCH --ntasks-per-node=56
#SBATCH --cpus-per-task=1
#SBATCH --exclusive
# hhcpu defaults to 1 GiB per allocated CPU (56 GiB per node); request the whole node memory.
#SBATCH --mem=0
#SBATCH --time=04:00:00
#SBATCH --output=wacomm-hh-test-%j.out
# Configure, build, and test the Serial, OpenMP, MPI, and MPI+OpenMP CPU builds on one exclusive
# hhcpu node (2 x Intel Xeon Gold 5520+, 56 cores). Usage, from the repository root:
#   sbatch examples/wacomm-sarno-lite/tools/hhcpu_backend_test_job.sh [build ...]
# Environment: WACOMM_PYTHON (interpreter with tools/requirements-figures.txt),
# WACOMM_DOWNLOADS (shared verified dependency archive directory), WACOMM_PERL5LIB (directories
# holding the Perl core modules that the private OpenSSL Configure needs; hhcpu lacks perl-core).
set -euo pipefail

repository=${SLURM_SUBMIT_DIR:-$(cd "$(dirname "$0")/../../.." && pwd)}
cd "$repository"
module load gcc-12.2.1/ompi-4.1.4_nccl
module load python/3.13.2
python=${WACOMM_PYTHON:-$HOME/devel/wacomm-py313/bin/python}
downloads=${WACOMM_DOWNLOADS:-$HOME/devel/wacomm-downloads}
export CC=gcc CXX=g++ OMPI_CC=gcc OMPI_CXX=g++
if [ -n "${WACOMM_PERL5LIB:-}" ]; then
    export PERL5LIB="$WACOMM_PERL5LIB${PERL5LIB:+:$PERL5LIB}"
fi

builds=("$@")
if [ ${#builds[@]} -eq 0 ]; then
    builds=(serial omp mpi mpi-omp)
fi

echo "revision=$(git rev-parse HEAD) host=$(hostname) date=$(date -u +%FT%TZ)"
g++ --version | head -1
mpirun --version | head -1
cmake --version | head -1
"$python" --version
lscpu | grep -E "Model name|^CPU\(s\)|NUMA node[0-9]"

status=0
for build in "${builds[@]}"; do
    case $build in
        serial) options=(-DUSE_OMP=OFF -DUSE_MPI=OFF) ;;
        omp) options=(-DUSE_OMP=ON -DUSE_MPI=OFF) ;;
        mpi) options=(-DUSE_OMP=OFF -DUSE_MPI=ON) ;;
        mpi-omp) options=(-DUSE_OMP=ON -DUSE_MPI=ON) ;;
        *) echo "unknown build $build" >&2; exit 2 ;;
    esac
    tree=build-hhcpu-$build
    echo "=== $build: configure $(date -u +%T)"
    cmake -S . -B "$tree" -DCMAKE_BUILD_TYPE=Release "${options[@]}" -DUSE_CUDA=OFF -DUSE_EMPI=OFF \
        -DUSE_OPENACC=OFF -DWACOMM_BOOTSTRAP_DEPENDENCIES=AUTO -DWACOMM_DEPENDENCY_DOWNLOAD_DIR="$downloads" \
        -DPython3_EXECUTABLE="$python" > "$tree.configure.log" 2>&1 || { tail -40 "$tree.configure.log"; status=1; continue; }
    echo "=== $build: build $(date -u +%T)"
    cmake --build "$tree" --parallel 56 > "$tree.build.log" 2>&1 || { tail -60 "$tree.build.log"; status=1; continue; }
    echo "=== $build: test $(date -u +%T)"
    # 56 Slurm tasks give Open MPI the slots its two-rank tests need. The private NetCDF builds no
    # utilities; the site module supplies ncgen/ncdump for test fixtures only, not for linking.
    (module load gcc-12.1.1/netcdf-c-4.9.3 && nc-config --version &&
        ctest --test-dir "$tree" --output-on-failure --timeout 1800) > "$tree.ctest.log" 2>&1 || status=1
    grep -E "tests passed|tests failed|\*\*\*|Not Run|Skipped|SKIP" "$tree.ctest.log" || true
done
echo "=== done status=$status $(date -u +%T)"
exit $status
