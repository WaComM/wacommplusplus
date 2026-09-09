# MPI example

## Scientific objective and prerequisites

Distribute a deterministic ROMS particle population across MPI ranks and confirm agreement with serial execution. Install the application dependencies plus a C MPI implementation, and provide `forcing.nc` and `sources.json` accessible to the launch environment.

## Configuration and command

Configure with `cmake -S . -B build-mpi -DUSE_MPI=ON`, build with `cmake --build build-mpi`, then run `mpiexec -n 4 ./build-mpi/wacommplusplus examples/parallel-mpi.json`. MPI partitions particles while the adapter and numerical equations remain unchanged.

## Expected behavior and validation

The gathered particle identities, states, total concentration, and output times must match a one-rank and serial run within tolerance. Test rank counts that do and do not divide the particle count. Seeded stochastic output must be decomposition-independent.

## Limitations, interpretation, and reproducibility

Performance depends on communication, decomposition, filesystem, and process placement; MPI is not a different scientific formulation. Record revision, input/configuration checksums, compiler, MPI implementation/version, CMake options, ranks and placement, tests, tolerances, timing, and output checksums. See [parallelism](../docs/parallelism.md).


## References

- Dagestad, K.-F., Röhrs, J., Breivik, Ø., and Ådlandsvik, B. (2018). OpenDrift v1.0: a generic framework for trajectory modelling. *Geoscientific Model Development*, 11, 1405–1420. [doi:10.5194/gmd-11-1405-2018](https://doi.org/10.5194/gmd-11-1405-2018).
- Montella, R., et al. (2023). A highly scalable high-performance Lagrangian transport and diffusion model for marine pollutants assessment. *Proceedings of PDP 2023*, 17–26. [doi:10.1109/PDP59025.2023.00012](https://doi.org/10.1109/PDP59025.2023.00012).
