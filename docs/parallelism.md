# Parallel execution

Enable one or more optional backends at configure time, for example `-DUSE_OMP=ON` or `-DUSE_MPI=ON`. MPI and EMPI are mutually exclusive. Concentration updates use an OpenMP atomic increment so particle counts are preserved when several particles occupy one cell. CUDA host reads synchronize after asynchronous device copies.

Record ranks, threads, affinity, GPU and driver data with results. Validate deterministic output against serial execution before scientific use.
