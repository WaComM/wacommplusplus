# WaComM++ examples

Examples are part of the same versioned WaComM++ scientific software product as physics, implementation, tests, and documentation. Existing scenario files demonstrate native and ROMS inputs, source definitions, MPI/OpenMP/CUDA launch scripts, and restart preparation.

For each run, inspect the JSON before use, replace local forcing paths, record input checksums and the Git revision, run the matching executable/backend, and validate particle counts and trajectories against a deterministic reference. Backtracking should normally use endpoint particles, `random:false`, and `backward_diffusion:none`. Stochastic backward output is an ensemble of candidate origins, not a unique reconstructed source.

Every scenario guide must state its scientific objective, prerequisites, adapter variables, configuration, exact command, expected behavior, validation, limitations, interpretation, metadata checklist, and related tests. Legacy examples without a same-name guide are retained but are not considered release-validated until documented to this standard.
