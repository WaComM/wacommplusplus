# WaComM++ examples

Examples are part of the same versioned WaComM++ scientific software product as physics, implementation, tests, and documentation. Existing scenario files demonstrate native and ROMS inputs, source definitions, MPI/OpenMP/CUDA launch scripts, and restart preparation.

For each run, inspect the JSON before use, replace local forcing paths, record input checksums and the Git revision, run the matching executable/backend, and validate particle counts and trajectories against a deterministic reference. Backtracking should normally use endpoint particles, `random:false`, and `backward_diffusion:none`. Stochastic backward output is an ensemble of candidate origins, not a unique reconstructed source.

Every scenario guide states its scientific objective, prerequisites, adapter variables, configuration, exact command, expected behavior, validation, limitations, interpretation, metadata checklist, and related tests. CTest enforces that every checked-in JSON artifact has a same-name Markdown guide.

Completed runs can be inspected with the documented [trajectory diagnostic workflow](../docs/trajectory-diagnostics.md). It consumes multiple single-time WaComM++ NetCDF snapshots and creates machine-readable spread statistics plus an annotated SVG map; it is a postprocessor and cannot change or validate solver physics.

The release-oriented set is `forward/backward-roms`, `forward/backward-nemo`, `forward/backward-hycom`, `forward/backward-wacomm`, `stochastic-forward`, `stochastic-backward-ensemble`, `restart-forward`, `restart-backward`, and the OpenMP, MPI, and CUDA parallel examples. Files named for historical webinars, oil-spill demonstrations, SAR exercises, or regional use cases retain their original scientific context and require the external datasets stated in their same-name guides. Source-only GeoJSON artifacts are not standalone run configurations; their guides show how to reference them from a complete configuration.

The leeway-enabled reference set includes catalog-mean, coefficient-ensemble, coupled WRF/WW3, and projected-wave forward/backward pairs. `sar-person-projected-wave-forward` and `sar-person-projected-wave-backward` require `USE_PROJ=ON` and demonstrate an explicit EPSG:3857 WW3 grid without implying that Web Mercator is scientifically preferred. Other historical files containing `sar` remain passive unless their JSON explicitly selects `drift.model=leeway`.

## Scholarly basis

The example narratives distinguish numerical verification from scientific validation and cite only peer-reviewed journal articles or peer-reviewed proceedings. Core references are Montella et al. (2023), [doi:10.1109/PDP59025.2023.00012](https://doi.org/10.1109/PDP59025.2023.00012); Dagestad et al. (2018), [doi:10.5194/gmd-11-1405-2018](https://doi.org/10.5194/gmd-11-1405-2018); Breivik et al. (2011), [doi:10.1016/j.apor.2011.01.005](https://doi.org/10.1016/j.apor.2011.01.005); and Thygesen (2011), [doi:10.1016/j.jmarsys.2011.03.009](https://doi.org/10.1016/j.jmarsys.2011.03.009). Each scenario guide selects the subset relevant to its modeled process.
