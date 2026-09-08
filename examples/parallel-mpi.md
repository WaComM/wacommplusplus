# MPI example

## Scientific objective and prerequisites

Distribute a deterministic ROMS particle population across MPI ranks and confirm agreement with serial execution. Install the application dependencies plus a C MPI implementation, and provide `forcing.nc` and `sources.json` accessible to the launch environment.

## Configuration and command

Configure with `cmake -S . -B build-mpi -DUSE_MPI=ON`, build with `cmake --build build-mpi`, then run `mpiexec -n 4 ./build-mpi/wacommplusplus examples/parallel-mpi.json`. MPI partitions particles while the adapter and numerical equations remain unchanged.

## Expected behavior and validation

The gathered particle identities, states, total concentration, and output times must match a one-rank and serial run within tolerance. Test rank counts that do and do not divide the particle count. Seeded stochastic output must be decomposition-independent.

## Limitations, interpretation, and reproducibility

Performance depends on communication, decomposition, filesystem, and process placement; MPI is not a different scientific formulation. Record revision, input/configuration checksums, compiler, MPI implementation/version, CMake options, ranks and placement, tests, tolerances, timing, and output checksums. See [parallelism](../docs/parallelism.md).
