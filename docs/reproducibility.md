# Reproducibility

Archive the Git commit/tag, complete configuration, forcing and restart filenames plus SHA-256 checksums, random seed, compiler and version, CMake cache/options, dependency versions, operating system and architecture, MPI implementation and ranks, OpenMP thread count and affinity, GPU/driver/CUDA details, test output, tolerances, and output checksums.

The configured seed is stable and must not be replaced by wall-clock time, rank, thread identity, or accelerator scheduling. CPU and CUDA stochastic displacement use a stateless key comprising the seed, stable particle identity, physical forcing interval start, absolute substep ordinal, and component. This makes the random variate independent of OpenMP scheduling, MPI decomposition, and CUDA scheduling. CPU and CUDA transcendental implementations may differ in their last floating-point bits, so comparison requires a declared tolerance and recorded toolchain.
