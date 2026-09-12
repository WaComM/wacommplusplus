# OpenMP example

## Scientific objective and prerequisites

Run the deterministic ROMS scenario with shared-memory particle parallelism and verify equivalence with serial execution. Install a C++17 compiler, NetCDF C/C++4, log4cplus, nlohmann-json, and a compiler-supported OpenMP runtime. Provide compatible `forcing.nc` and `sources.json`.

## Configuration and command

The JSON keeps physics independent of thread scheduling. Configure with `cmake -S . -B build-omp -DUSE_OMP=ON`, build with `cmake --build build-omp`, then run `OMP_NUM_THREADS=4 ./build-omp/wacommplusplus examples/parallel-openmp/parallel-openmp.json`. AppleClang users must first install the runtime with `brew install libomp`; CMake discovers that keg from Homebrew without assuming `/opt/homebrew` or `/usr/local`.

## Expected behavior and validation

Particle motion and total concentration count must match a serial build within the declared numerical tolerance. Repeat with one, two, and four threads; seeded stochastic cases must also remain independent of thread count.

## Limitations, interpretation, and reproducibility

Speedup depends on particle count, memory bandwidth, compiler runtime, and affinity. OpenMP changes scheduling, not the physical model. Archive revision, input/configuration checksums, compiler/OpenMP versions, CMake cache, thread count and affinity, tests, tolerances, timings, and output checksums. See [parallelism](../../docs/parallelism.md).


## References

- Dagestad, K.-F., Röhrs, J., Breivik, Ø., and Ådlandsvik, B. (2018). OpenDrift v1.0: a generic framework for trajectory modelling. *Geoscientific Model Development*, 11, 1405–1420. [doi:10.5194/gmd-11-1405-2018](https://doi.org/10.5194/gmd-11-1405-2018).
- Montella, R., et al. (2023). A highly scalable high-performance Lagrangian transport and diffusion model for marine pollutants assessment. *Proceedings of PDP 2023*, 17–26. [doi:10.1109/PDP59025.2023.00012](https://doi.org/10.1109/PDP59025.2023.00012).
