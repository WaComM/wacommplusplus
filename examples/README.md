# WaComM++ examples

Examples are part of the same versioned WaComM++ scientific software product as physics, implementation, tests, and documentation. Existing scenario files demonstrate native and ROMS inputs, source definitions, MPI/OpenMP/CUDA launch scripts, and restart preparation.

For each run, inspect the JSON before use, replace local forcing paths, record input checksums and the Git revision, run the matching executable/backend, and validate particle counts and trajectories against a deterministic reference. Backtracking should normally use endpoint particles, `random:false`, and `backward_diffusion:none`. Stochastic backward output is an ensemble of candidate origins, not a unique reconstructed source.

Every scenario guide states its scientific objective, prerequisites, adapter variables, configuration, exact command, expected behavior, validation, limitations, interpretation, metadata checklist, and related tests. CTest enforces that every checked-in JSON artifact has a same-name Markdown guide.

The release-oriented set is `forward/backward-roms`, `forward/backward-nemo`, `forward/backward-hycom`, `forward/backward-wacomm`, `stochastic-forward`, `stochastic-backward-ensemble`, `restart-forward`, `restart-backward`, and the OpenMP, MPI, and CUDA parallel examples. Files named for historical webinars, oil-spill demonstrations, SAR exercises, or regional use cases retain their original scientific context and require the external datasets stated in their same-name guides. Source-only GeoJSON artifacts are not standalone run configurations; their guides show how to reference them from a complete configuration.
