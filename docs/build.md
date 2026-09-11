# Build guide

Install a C++17 compiler and CMake 3.20 or newer. By default CMake prefers installed NetCDF C/C++4 (with DAP2/DAP4), log4cplus, nlohmann-json, and pkg-config, then builds missing application dependencies privately below the build tree:

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
cmake --install build --prefix ./install
```

The dependency-free numerical tests can be built without application libraries:

```bash
cmake -S . -B build-core -DBUILD_APPLICATION=OFF
cmake --build build-core
ctest --test-dir build-core --output-on-failure
```

Physical Raspberry Pi ARM64 and RV64 builds use the same portable source and complete serial test suite. The [Raspberry Pi OS ARM64](raspberry-pi.md) and [RISC-V 64 Linux](riscv64-linux.md) guides define native-board preflight checks, dependencies, self-hosted runner labels, validation gates, and evidence retention. Cross-compilation, an ARM64 proxy, or QEMU execution does not by itself demonstrate native hardware execution.

Optional switches are `USE_OMP`, `USE_MPI`, `USE_EMPI`, `USE_OPENACC`, `USE_CUDA`, `USE_PROJ`, and `BUILD_REMOTE_INTEGRATION_TESTS`. `USE_PROJ=ON` enables explicitly declared projected environmental grids and requires the PROJ library and coordinate database; it is not required by the portable core or geographic adapters. Application builds require NetCDF with DAP2 and DAP4 support so configured remote datasets cannot fail later merely because transport was omitted at build time. `BUILD_REMOTE_INTEGRATION_TESTS=ON` adds the network-dependent official OPeNDAP hyperslab test; it remains off in the default offline suite. CUDA is unavailable on modern macOS. Configuration fails explicitly when a requested dependency is missing.

## Dependency bootstrap

`WACOMM_BOOTSTRAP_DEPENDENCIES` is a three-state cache string. `AUTO` is the default and prefers a usable installed stack. If installed NetCDF-C, NetCDF-C++4, or DAP support is missing, it builds a coherent private zlib, libaec/SZIP, TLS-enabled curl, HDF5, NetCDF-C, and NetCDF-C++4 chain; JSON and log4cplus fall back independently. `ON` bypasses installed copies and always uses the pinned private stack. `OFF` performs no dependency downloads and fails during configuration when a required installed package or DAP capability is unavailable. Private files remain under `<build>/external` and are never installed globally.

```bash
cmake -S . -B build -DWACOMM_BOOTSTRAP_DEPENDENCIES=AUTO
cmake -S . -B build-private -DWACOMM_BOOTSTRAP_DEPENDENCIES=ON
cmake -S . -B build-system -DWACOMM_BOOTSTRAP_DEPENDENCIES=OFF
```

The private curl retains HTTPS: Schannel is used on Windows, Secure Transport on macOS, and OpenSSL on other Unix platforms. Windows dependencies use native CMake builds with Visual Studio or Ninja; libaec supplies the SZIP-compatible `libsz` interface. A forced Unix private build therefore also requires OpenSSL development files. PROJ, OpenMP, CUDA, and FlexMPI remain optional system/toolchain facilities rather than members of the private stack.

`WACOMM_BOOTSTRAP_PARALLEL_IO=ON` forces the private HDF5/NetCDF portion even in `AUTO`, requires discoverable MPI C and C++ support, enables parallel HDF5 and NetCDF-4 I/O, and propagates MPI for their static libraries independently of solver-level `USE_MPI`. `WACOMM_BOOTSTRAP_DEPENDENCIES=OFF` with parallel bootstrap is rejected explicitly. This option controls library capability; `USE_MPI` separately controls WaComM++ particle decomposition.

```bash
cmake -S . -B build-parallel-io \
  -DWACOMM_BOOTSTRAP_DEPENDENCIES=AUTO \
  -DWACOMM_BOOTSTRAP_PARALLEL_IO=ON
cmake --build build-parallel-io --parallel
ctest --test-dir build-parallel-io --output-on-failure
```

The selected bootstrap and parallel-I/O modes appear in configure output, the CMake cache, and WaComM++ reproducibility metadata. Archive them with the dependency versions and output checksums. A successful build verifies dependency integration, not numerical or observational validation.

On macOS with AppleClang, install Homebrew's keg-only OpenMP runtime with `brew install libomp`. When `USE_OMP=ON`, configuration obtains the formula prefix from Homebrew and supplies its header, library, and AppleClang frontend flags to CMake's imported `OpenMP::OpenMP_CXX` target. Existing command-line `OpenMP_CXX_*` and `OpenMP_omp_LIBRARY` cache settings take precedence. If Homebrew is unavailable, configure those variables explicitly or select a compiler with its own OpenMP runtime before creating the build directory.

On Windows, either use automatic/private bootstrap or install dependencies through the checked-in vcpkg manifest and require that stack with `WACOMM_BOOTSTRAP_DEPENDENCIES=OFF`:

```powershell
vcpkg install --triplet x64-windows
cmake -S . -B build -A x64 `
  -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" `
  -DWACOMM_BOOTSTRAP_DEPENDENCIES=OFF
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

`USE_EMPI=ON` requires an MPI implementation plus a real FlexMPI installation. Configuration searches for `empi.h` and the EMPI library and fails clearly if either is absent. Pass its installation prefix through `CMAKE_PREFIX_PATH`, `CMAKE_INCLUDE_PATH`, or `CMAKE_LIBRARY_PATH` when it is outside the system search path.


## References

- Sandve, G. K., Nekrutenko, A., Taylor, J., and Hovig, E. (2013). Ten simple rules for reproducible computational research. *PLoS Computational Biology*, 9, e1003285. [doi:10.1371/journal.pcbi.1003285](https://doi.org/10.1371/journal.pcbi.1003285).
- Montella, R., et al. (2023). A highly scalable high-performance Lagrangian transport and diffusion model for marine pollutants assessment. *Proceedings of PDP 2023*, 17–26. [doi:10.1109/PDP59025.2023.00012](https://doi.org/10.1109/PDP59025.2023.00012).
