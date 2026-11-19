#!/bin/bash
# Configure, build, test, and benchmark the WaComM++ backend builds on one host profile.
# Usage, from the repository root (under sbatch on Slurm hosts, directly elsewhere):
#   backend_matrix_job.sh PROFILE test [build ...]
#   backend_matrix_job.sh PROFILE bench RATE SUITE CONFIG...
# PROFILE selects the toolchain of a measured host:
#   rainbow         AMD Ryzen 9 9900X (12 cores, SMT), RTX 4080 SUPER, sm_89, Slurm partition gpu
#   sunbow          AMD Ryzen 9 9900X (12 cores, SMT), RTX PRO 6000 Blackwell, sm_120, no Slurm
#   purplejeans-gn  PurpleJeans low-gn: 2 x Xeon Gold 5218 (32 cores), 4 x V100-SXM2, sm_70
#   purplejeans-wn  PurpleJeans low-wn: 2 x Xeon Gold 5218 (32 cores), CPU only
#   local           the compilers on PATH (CPU builds only)
# CONFIG is BUILD:P:N:G as in sarno_backend_matrix.py; the build trees are build-PROFILE-BUILD.
# Benchmarks bind ranks and threads to physical cores (OMP_PLACES=cores), so P*N must not exceed
# the physical cores of a node; SMT siblings stay idle.
# Environment: WACOMM_PYTHON (interpreter with tools/requirements-figures.txt), WACOMM_DOWNLOADS
# (verified dependency archive directory), WACOMM_FORCING (directory with processed-6h/ and
# prepared-forcing.sha256), WACOMM_MATRIX_OPTIONS (--io-async, --io-prefetch, --interleave,
# --spare-cores=N), WACOMM_PERL5LIB (directories holding the Perl core modules that the private
# OpenSSL Configure needs where the compute nodes lack perl-core, as on PurpleJeans), WACOMM_MATRIX_ROOT
# (directory of the benchmark suites, default data/matrix-PROFILE; a 10^6 particles/hour suite needs about
# 8 GiB, so point it at project storage where the home directory has a small quota, as on rainbow).
# Slurm examples:
#   ssh rainbow 'bash -lc "sbatch -p gpu --exclusive --gres=gpu:1 -t 6:00:00 tools/backend_matrix_job.sh rainbow test"'
#   sbatch -p low-gn -N1 --ntasks-per-node=32 --exclusive --mem=0 --gres=gpu:tesla:4 backend_matrix_job.sh purplejeans-gn test
set -euo pipefail

repository=${SLURM_SUBMIT_DIR:-$(cd "$(dirname "$0")/../../.." && pwd)}
cd "$repository"
profile=$1
mode=$2
shift 2

cuda_home=
arch=
cores=$(getconf _NPROCESSORS_ONLN)
case $profile in
    rainbow)
        module purge
        module load sw/cmake-3.31.6 comps/gcc-13.3.0/openmpi-4.1.8 comps/nvidia/nvhpc-byo-compiler/25.3
        # The HPC SDK cuda/ directory symlinks bin, include, lib64 only; CMake needs the real toolkit.
        cuda_home=$(dirname "$(dirname "$(readlink -f "$(command -v nvcc)")")")
        export CC=gcc-13 CXX=g++-13 OMPI_CC=gcc-13 OMPI_CXX=g++-13
        arch=89; cores=12
        python=${WACOMM_PYTHON:-$HOME/devel/wacomm-py311/bin/python}
        forcing=${WACOMM_FORCING:-$HOME/devel/matrix/forcing} ;;
    sunbow)
        # sunbow has the driver only: private CMake, Open MPI, and the CUDA 12.8 toolkit of the
        # NVIDIA HPC SDK 25.3 under ~/opt/wacomm-toolchain; system GCC 13.
        toolchain=$HOME/opt/wacomm-toolchain
        cuda_home=${WACOMM_CUDA_HOME:-$toolchain/cuda-12.8}
        export OPAL_PREFIX=$toolchain/openmpi PATH=$toolchain/openmpi/bin:$toolchain/cmake/bin:$cuda_home/bin:$PATH
        export LD_LIBRARY_PATH=$toolchain/openmpi/lib:$toolchain/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}
        export CC=gcc-13 CXX=g++-13 OMPI_CC=gcc-13 OMPI_CXX=g++-13
        arch=120; cores=12
        python=${WACOMM_PYTHON:-$HOME/devel/wacomm-py312/bin/python}
        forcing=${WACOMM_FORCING:-$HOME/devel/wacomm-matrix-data/wacomm-sarno-lite} ;;
    purplejeans-gn|purplejeans-wn)
        module load gcc-12.2.1/ompi-4.1.4_nccl
        if [ "$profile" = purplejeans-gn ]; then
            module load nvidia/cuda-12.8.0
            cuda_home=$(dirname "$(dirname "$(readlink -f "$(command -v nvcc)")")")
            arch=70
        fi
        # The site UCX fails on these nodes; Open MPI uses the ob1 PML with shared memory and TCP.
        # HCOLL opens its own UCX endpoints, and on two or more nodes one rank can spin forever in
        # hmca_bcol_ucx_p2p_disconnect inside MPI_Finalize, so the HCOLL collectives stay off too.
        export OMPI_MCA_pml=ob1 OMPI_MCA_btl=self,vader,tcp OMPI_MCA_coll_hcoll_enable=0
        export CC=gcc CXX=g++ OMPI_CC=gcc OMPI_CXX=g++
        cores=32
        python=${WACOMM_PYTHON:-$HOME/devel/wacomm-py311/bin/python}
        forcing=${WACOMM_FORCING:-$HOME/devel/matrix/forcing} ;;
    local)
        export CC=${CC:-cc} CXX=${CXX:-c++}
        python=${WACOMM_PYTHON:-python3}
        forcing=${WACOMM_FORCING:-data/wacomm-sarno-lite} ;;
    *) echo "unknown profile $profile" >&2; exit 2 ;;
esac
downloads=${WACOMM_DOWNLOADS:-$HOME/devel/wacomm-downloads}
matrix_root=${WACOMM_MATRIX_ROOT:-data/matrix-$profile}
if [ -n "${WACOMM_PERL5LIB:-}" ]; then
    export PERL5LIB="$WACOMM_PERL5LIB${PERL5LIB:+:$PERL5LIB}"
fi
if [ -n "$cuda_home" ]; then
    export PATH=$cuda_home/bin:$PATH
fi

echo "revision=$(git rev-parse HEAD) profile=$profile host=$(hostname) job=${SLURM_JOB_ID:-none} date=$(date -u +%FT%TZ)"
git status --porcelain
"$CXX" --version | head -1
mpirun --version 2>/dev/null | head -1 || true
cmake --version | head -1
"$python" --version
lscpu | grep -E "Model name|^CPU\(s\)|Thread\(s\) per core|NUMA node[0-9]" || true
if [ -n "$cuda_home" ]; then
    nvcc --version | tail -1
    nvidia-smi --query-gpu=index,name,pci.bus_id,driver_version,memory.total,compute_cap --format=csv
fi
env | grep -E "^OMPI_MCA_|^SLURM_JOB_(NODELIST|NUM_NODES|CPUS)" | sort || true

builds_of_profile() {
    echo serial omp mpi mpi-omp
    if [ -n "$cuda_home" ]; then
        echo cuda omp-cuda mpi-cuda mpi-omp-cuda
    fi
}

test_builds() {
    local builds=("$@") status=0 build tree options sanitizer
    if [ ${#builds[@]} -eq 0 ]; then
        read -r -a builds <<< "$(builds_of_profile | tr '\n' ' ')"
    fi
    sanitizer=
    for candidate in "$cuda_home/compute-sanitizer/compute-sanitizer" "$cuda_home/bin/compute-sanitizer"; do
        if [ -n "$cuda_home" ] && [ -x "$candidate" ] && [ -z "$sanitizer" ]; then
            sanitizer=$candidate
        fi
    done
    for build in "${builds[@]}"; do
        options=(-DUSE_OMP=OFF -DUSE_MPI=OFF -DUSE_CUDA=OFF)
        case $build in *omp*) options[0]=-DUSE_OMP=ON ;; esac
        case $build in mpi*) options[1]=-DUSE_MPI=ON ;; esac
        case $build in
            *cuda)
                options[2]=-DUSE_CUDA=ON
                options+=(-DCMAKE_CUDA_COMPILER="$cuda_home/bin/nvcc" -DCUDAToolkit_ROOT="$cuda_home"
                          -DCMAKE_CUDA_HOST_COMPILER="$CXX" -DCMAKE_CUDA_ARCHITECTURES="$arch") ;;
        esac
        tree=build-$profile-$build
        echo "=== $build: configure $(date -u +%T)"
        cmake -S . -B "$tree" -DCMAKE_BUILD_TYPE=Release "${options[@]}" -DUSE_EMPI=OFF -DUSE_OPENACC=OFF \
            -DCMAKE_C_COMPILER="$CC" -DCMAKE_CXX_COMPILER="$CXX" \
            -DWACOMM_BOOTSTRAP_DEPENDENCIES=AUTO -DWACOMM_DEPENDENCY_DOWNLOAD_DIR="$downloads" \
            -DPython3_EXECUTABLE="$python" > "$tree.configure.log" 2>&1 || { tail -40 "$tree.configure.log"; status=1; continue; }
        echo "=== $build: build $(date -u +%T)"
        cmake --build "$tree" --parallel "$cores" > "$tree.build.log" 2>&1 \
            || { grep -nE "error|Error" "$tree.build.log" | head -40; status=1; continue; }
        echo "=== $build: test $(date -u +%T)"
        OMP_NUM_THREADS=2 ctest --test-dir "$tree" --output-on-failure --timeout 1800 > "$tree.ctest.log" 2>&1 || status=1
        grep -E "tests passed|tests failed|\*\*\*|Not Run|Skipped|SKIP" "$tree.ctest.log" || true
        grep -hoE "CUDA HOST2DEV sec: [^ ]+ records: [0-9]+ reused: [01]" "$tree/Testing/Temporary/LastTest.log" 2>/dev/null | head -4 || true
        if [ "$build" = cuda ] || [ "$build" = mpi-omp-cuda ]; then
            for program in cuda_particle_parity_test cuda_device_forcing_test; do
                if [ -z "$sanitizer" ]; then
                    echo "memcheck SKIPPED: no compute-sanitizer under $cuda_home"; status=1; break
                fi
                echo "=== $build: memcheck $program $(date -u +%T)"
                (cd "$tree" && OMP_NUM_THREADS=1 "$sanitizer" --tool memcheck --error-exitcode 1 "./$program") \
                    > "$tree.$program.memcheck.log" 2>&1 || status=1
                tail -n 2 "$tree.$program.memcheck.log"
            done
        fi
    done
    echo "=== done status=$status $(date -u +%T)"
    return $status
}

bench() {
    local rate=$1 suite=$2 arguments=() build option
    shift 2
    for config in "$@"; do
        arguments+=(--config "$config")
    done
    for build in $(builds_of_profile); do
        if [ -x "build-$profile-$build/wacommplusplus" ]; then
            arguments+=(--build "$build=build-$profile-$build/wacommplusplus")
        fi
    done
    for option in ${WACOMM_MATRIX_OPTIONS:-}; do
        case "$option" in
            --io-async|--io-prefetch|--interleave|--spare-cores=[0-9]*) arguments+=("$option") ;;
            *) echo "unsupported WACOMM_MATRIX_OPTIONS entry: $option" >&2; exit 2 ;;
        esac
    done
    uptime
    echo "suite=$matrix_root/$suite rate=$rate options=${WACOMM_MATRIX_OPTIONS:-none} forcing=$forcing"
    "$python" -I examples/wacomm-sarno-lite/tools/sarno_backend_matrix.py run --root "$matrix_root/$suite" \
        --rate "$rate" --blocks 3 --forcing "$forcing" --prune-snapshots --bind --nodes "${SLURM_JOB_NUM_NODES:-1}" \
        --python "$python" "${arguments[@]}"
}

case $mode in
    test) test_builds "$@" ;;
    bench) bench "$@" ;;
    *) echo "unknown mode $mode" >&2; exit 2 ;;
esac
