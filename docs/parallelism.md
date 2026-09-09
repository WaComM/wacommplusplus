# Parallel execution

Enable one or more optional backends at configure time, for example `-DUSE_OMP=ON` or `-DUSE_MPI=ON`. MPI and EMPI are mutually exclusive. Concentration updates use an OpenMP atomic increment so particle counts are preserved when several particles occupy one cell. CUDA allocations, transfers, launches, synchronization, events and releases are checked for runtime errors. Host reads synchronize after asynchronous device copies, and per-thread event timings are stored independently before aggregation.

Record ranks, threads, affinity, GPU and driver data with results. Validate deterministic output against serial execution before scientific use.
