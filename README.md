# WaComM++

WaComM++ (Water quality COMmunity Model in C++) is a C++17 Lagrangian transport and diffusion framework for marine pollutants and drifting surface objects. Gridded Eulerian ocean forcing drives compact, independently advected particles whose state includes position, emission time, age, health, a stable 64-bit identity, and restart-safe drift-object metadata.

The current release reads ROMS, NEMO, HYCOM, and native WACOMM products through direction-neutral adapters. It supports deterministic forward and backward traversal, configurable stochastic forward diffusion, an explicitly labelled stochastic backward ensemble mode, restart input/output, OpenMP, MPI, FlexMPI/EMPI, OpenACC, and optional CUDA. Unsupported adapter layouts are rejected rather than silently interpreted as another grid.

Surface-object tracking augments the ambient current with an empirically parameterized leeway velocity resolved into downwind and crosswind components relative to 10 m wind, plus optional surface Stokes drift. Generic weather and wave interfaces keep transport physics independent of forcing products; the initial concrete adapters read WRF wind and WAVEWATCH III Stokes velocity. The first object catalog comprises a person in water, liferafts with and without drogues, a generic vessel, and a shipping container. An opt-in coefficient ensemble assigns reproducible catalog-residual velocities from the configured seed and stable particle identity. Passive transport remains the default. The same member velocity is evaluated in forward and backward integrations; only the solver applies temporal orientation.

Environmental adapters validate declared velocity and coordinate units and normalize supported CF numeric time coordinates to the WaComM physical epoch. Ambiguous units, unsupported calendars, unequal grids, and inconsistent timestamps are rejected before particle integration.

Environmental grids remain exact-match by default. Explicit geographic options interpolate Earth-relative WRF or WW3 vectors from rectilinear or valid curvilinear grids without extrapolation, with deterministic indexed curvilinear lookup. Optional `USE_PROJ=ON` adds declared-CRS transformation for rectilinear or valid curvilinear projected grids with metric `x/y` coordinates. A separate first-order conservative operator remaps rectilinear geographic cell averages by spherical overlap; it is not selectable for point-valued wind or Stokes velocity. Coordinate transformation never substitutes for vector rotation.

Backward deterministic tracking reverses forcing traversal and resolved/terminal motion and suppresses normal forward sources. It can identify candidate prior locations under the supplied circulation and model assumptions; stochastic backward tracking is not a unique inverse trajectory.

A read-only [trajectory diagnostic workflow](docs/trajectory-diagnostics.md) joins stable particle identities across output snapshots, computes explicitly descriptive ensemble-spread statistics, and creates a self-documenting SVG map with projection, extent, time, provenance, and uncertainty semantics. It does not infer probability contours or modify solver physics.

## Quick start

Install CMake 3.20+, a C++17 compiler, NetCDF C/C++, log4cplus, nlohmann-json, pkg-config, and any requested backend. Then run:

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
./build/wacommplusplus examples/forward-roms.json
```

For the dependency-free portable numerical tests:

```bash
cmake -S . -B build-core -DBUILD_APPLICATION=OFF
cmake --build build-core
ctest --test-dir build-core --output-on-failure
```

Options include `USE_OMP`, `USE_MPI`, `USE_EMPI`, `USE_OPENACC`, `USE_CUDA`, `USE_PROJ`, `DEBUG`, `BUILD_APPLICATION`, and `BUILD_TESTING`. MPI and EMPI are mutually exclusive. CUDA is unavailable on modern macOS; PROJ is optional and required only for projected environmental grids.

## Reproducible operation

Set `physics.random_seed` explicitly and archive the complete configuration, Git revision, forcing and restart checksums, compiler/dependency versions, platform, parallel settings, tolerances, test output, and result checksums. Production random behavior does not use wall-clock time. Examples, documentation, tests, source implementation, and physical definitions are versioned together.

## Documentation and examples

Start at the [documentation index](docs/README.md) for the [model](docs/model.md), [build guide](docs/build.md), [configuration](docs/configuration.md), [adapters](docs/adapters.md), [OpenDrift comparison and roadmap](docs/opendrift-comparison.md), [backtracking](docs/backtracking.md), [restart](docs/restart.md), [testing](docs/testing.md), [parallelism](docs/parallelism.md), [supported platforms](docs/supported-platforms.md), [reproducibility](docs/reproducibility.md), and [trajectory diagnostics](docs/trajectory-diagnostics.md). The [examples index](examples/README.md) covers every checked-in run configuration and source artifact, including forward/backward ROMS, NEMO, HYCOM, and native WACOMM workflows.

The [surface-drift guide](docs/sar-drift.md) defines the object-relative velocity model, coefficient provenance, units, ensemble assumptions, direction semantics, restart behavior, supported object classes, and current limitations. Its paired catalog-mean [forward](examples/sar-person-forward.md) and [backward](examples/sar-person-backward.md) scenarios provide uniform-wind references; the coefficient-ensemble [forward](examples/sar-person-ensemble-forward.md) and [backward](examples/sar-person-ensemble-backward.md) scenarios demonstrate reproducible member uncertainty; the coupled [forward](examples/sar-person-wrf-ww3-forward.md) and [backward](examples/sar-person-wrf-ww3-backward.md) scenarios exercise WRF wind and WW3 Stokes drift.

## Citation

Montella, R., et al. (2023), “A highly scalable high-performance Lagrangian transport and diffusion model for marine pollutants assessment,” *31st Euromicro International Conference on Parallel, Distributed and Network-Based Processing*, 17–26. Earlier WaComM publications and ocean-model references are listed in [references](docs/references.md).

Scientific use of surface-object drift should additionally cite the peer-reviewed leeway methodology and object-specific studies identified in [references](docs/references.md). Model output is conditional on forcing, parameterization, resolution, boundary treatment, and numerical configuration; it is not an observation or a unique reconstruction of an unobserved trajectory.

## License

WaComM++ is distributed under the terms in [LICENSE](LICENSE).
