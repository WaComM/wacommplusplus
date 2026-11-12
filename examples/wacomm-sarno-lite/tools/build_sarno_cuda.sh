#!/bin/bash
set -euo pipefail

repository=$(cd "$(dirname "$0")/../../.." && pwd)
cd "$repository"
module load openssl/openssl-4.0.2
module load cmake/cmake-4.4.3
module load gcc-12.2.1/ompi-4.1.4_nccl
module load nvidia/cuda-12.8.0
test -f build/_deps/wacomm_json-src/CMakeLists.txt
test -f build/external/lib/libnetcdf.a
test -f build/external/lib/liblog4cplusS.a
mkdir -p build-cuda/local-lib
ln -sfn "$repository/build/external/lib/liblog4cplusS.a" build-cuda/local-lib/liblog4cplus.a
export PKG_CONFIG_PATH="$repository/build/external/lib/pkgconfig:${PKG_CONFIG_PATH:-}"
cmake -S . -B build-cuda -DCMAKE_BUILD_TYPE=Release -DCMAKE_CUDA_ARCHITECTURES=70 \
    -DUSE_MPI=ON -DUSE_OMP=ON -DUSE_CUDA=ON -DUSE_EMPI=OFF -DUSE_OPENACC=OFF \
    -DWACOMM_BOOTSTRAP_DEPENDENCIES=AUTO -DWACOMM_BOOTSTRAP_PARALLEL_IO=OFF \
    -DCMAKE_PREFIX_PATH="$repository/build/external" \
    -DFETCHCONTENT_SOURCE_DIR_WACOMM_JSON="$repository/build/_deps/wacomm_json-src" \
    '-DMPI_CXX_COMPILER_INCLUDE_DIRS=/opt/share/comps/gcc-12.2.1/ompi-4.1.4_nccl/include;/opt/share/comps/gcc-12.2.1/ompi-4.1.4_nccl/include/openmpi;/opt/share/libs/gcc-8.5.0/hwloc-2.10.0/include' \
    -DMPI_CXX_HEADER_DIR=/opt/share/comps/gcc-12.2.1/ompi-4.1.4_nccl/include \
    -DCMAKE_CXX_STANDARD_LIBRARIES="$repository/build/external/lib/libhdf5_hl.a $repository/build/external/lib/libhdf5.a $repository/build/external/lib/libcurl.a $repository/build/external/lib/libz.a $repository/build/external/lib/libsz.a $repository/build/external/lib/libssl.a $repository/build/external/lib/libcrypto.a -ldl"
LIBRARY_PATH="$repository/build-cuda/local-lib:${LIBRARY_PATH:-}" \
    cmake --build build-cuda --target wacommplusplus --parallel 8
