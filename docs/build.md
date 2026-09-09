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

On Windows, install dependencies through the checked-in vcpkg manifest and use native MSVC:

```powershell
vcpkg install --triplet x64-windows
cmake -S . -B build -A x64 `
  -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

`USE_EMPI=ON` requires an MPI implementation plus a real FlexMPI installation. Configuration searches for `empi.h` and the EMPI library and fails clearly if either is absent. Pass its installation prefix through `CMAKE_PREFIX_PATH`, `CMAKE_INCLUDE_PATH`, or `CMAKE_LIBRARY_PATH` when it is outside the system search path.


## References

- Sandve, G. K., Nekrutenko, A., Taylor, J., and Hovig, E. (2013). Ten simple rules for reproducible computational research. *PLoS Computational Biology*, 9, e1003285. [doi:10.1371/journal.pcbi.1003285](https://doi.org/10.1371/journal.pcbi.1003285).
- Montella, R., et al. (2023). A highly scalable high-performance Lagrangian transport and diffusion model for marine pollutants assessment. *Proceedings of PDP 2023*, 17–26. [doi:10.1109/PDP59025.2023.00012](https://doi.org/10.1109/PDP59025.2023.00012).
