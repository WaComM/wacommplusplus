# Parallel execution

Enable one or more optional backends at configure time, for example `-DUSE_OMP=ON` or `-DUSE_MPI=ON`. MPI and EMPI are mutually exclusive. Concentration updates use an OpenMP atomic increment so particle counts are preserved when several particles occupy one cell. CUDA copies optional normalized WRF wind and WW3 Stokes arrays alongside the ocean window and passes null fields only when the corresponding adapter is disabled. Allocations, transfers, launches, synchronization, events and releases are checked for runtime errors. Host reads synchronize after asynchronous device copies, and per-thread event timings are stored independently before aggregation.

MPI transfers the fixed-width particle identity, six double-precision state fields, 16-bit object identifier, and 8-bit crosswind orientation through one shared derived datatype definition whose resized extent equals the complete particle structure. The MPI regression intentionally assigns a remainder to rank zero, verifies lossless scatter/gather ordering including drift metadata, and compares the distributed seeded update with serial execution.

Fast CI compiles serial, OpenMP, MPI, and combined MPI/OpenMP application configurations. Scheduled validation compiles MPI, OpenMP, and CUDA together, tests OpenACC, and runs every CPU-side regression, including MPI execution. The CUDA regression compares deterministic, seeded-stochastic, dynamic-WRF, and coupled WRF/WW3 CPU/GPU state and skips only when a device is absent. The manual GPU workflow runs it on a labeled self-hosted NVIDIA runner; the FlexMPI workflow requires an installation providing the actual EMPI runtime. Compile-only accelerator results are not numerical parity evidence.

Record ranks, threads, affinity, GPU and driver data with results. Validate deterministic output against serial execution before scientific use.

The [two-process Sarno Slurm example](../examples/wacomm-sarno-lite.md#two-process-mpi-calculation) runs the six-hour seeded ROMS calculation on `high-wn` with OpenMP/CUDA disabled. `bash tools/run_sarno_lite.sh --mpi` requests two cores on one node and uses OpenMPI with explicit core binding. Its five particle snapshots match the serial run exactly after identity ordering; this evidence covers the documented case and excludes inter-node communication performance and observational validation.

## References

- Montella, R., et al. (2023). A highly scalable high-performance Lagrangian transport and diffusion model for marine pollutants assessment. *Proceedings of PDP 2023*, 17–26. [doi:10.1109/PDP59025.2023.00012](https://doi.org/10.1109/PDP59025.2023.00012).
- Dagestad, K.-F., Röhrs, J., Breivik, Ø., and Ådlandsvik, B. (2018). OpenDrift v1.0: a generic framework for trajectory modelling. *Geoscientific Model Development*, 11, 1405–1420. [doi:10.5194/gmd-11-1405-2018](https://doi.org/10.5194/gmd-11-1405-2018).
