# Reproducibility

Archive the Git commit/tag, complete configuration, forcing and restart filenames plus SHA-256 checksums, random seed, compiler and version, CMake cache/options, dependency versions, operating system and architecture, MPI implementation and ranks, OpenMP thread count and affinity, GPU/driver/CUDA details, test output, tolerances, and output checksums.

The configured seed is stable and must not be replaced by wall-clock time, rank, thread identity, or accelerator scheduling. A stable particle identity and physical forcing interval are the basis for backend-independent stochastic keys.
