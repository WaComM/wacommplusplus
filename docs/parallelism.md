# Parallel execution

Enable one or more optional backends at configure time, for example `-DUSE_OMP=ON` or `-DUSE_MPI=ON`. MPI and EMPI are mutually exclusive. Concentration updates use an OpenMP atomic increment so particle counts are preserved when several particles occupy one cell. CUDA allocations, transfers, launches, synchronization, events and releases are checked for runtime errors. Host reads synchronize after asynchronous device copies, and per-thread event timings are stored independently before aggregation.

MPI transfers the fixed-width particle identity and six double-precision state fields through one shared derived datatype definition. The MPI regression intentionally assigns a remainder to rank zero, verifies lossless scatter/gather ordering, and compares the distributed seeded update with serial execution.

Fast CI compiles serial, OpenMP, MPI, and combined MPI/OpenMP application configurations. Scheduled validation compiles MPI, OpenMP, and CUDA together, tests OpenACC, and runs every CPU-side regression, including MPI execution. The CUDA regression compares deterministic and seeded-stochastic CPU/GPU state and skips only when a device is absent. The manual GPU workflow runs it on a labeled self-hosted NVIDIA runner; the FlexMPI workflow requires an installation providing the actual EMPI runtime. Compile-only accelerator results are not numerical parity evidence.

Record ranks, threads, affinity, GPU and driver data with results. Validate deterministic output against serial execution before scientific use.
