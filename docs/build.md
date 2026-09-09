# Build guide

Install a C++17 compiler, CMake 3.20 or newer, NetCDF C/C++, log4cplus, nlohmann-json, and pkg-config. Then run:

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

Optional switches are `USE_OMP`, `USE_MPI`, `USE_EMPI`, `USE_OPENACC`, and `USE_CUDA`. CUDA is unavailable on modern macOS. Configuration fails explicitly when a requested dependency is missing.

On macOS with AppleClang, install Homebrew's keg-only OpenMP runtime with `brew install libomp`. When `USE_OMP=ON`, configuration obtains the formula prefix from Homebrew and supplies its header, library, and AppleClang frontend flags to CMake's imported `OpenMP::OpenMP_CXX` target. Existing command-line `OpenMP_CXX_*` and `OpenMP_omp_LIBRARY` cache settings take precedence. If Homebrew is unavailable, configure those variables explicitly or select a compiler with its own OpenMP runtime before creating the build directory.
