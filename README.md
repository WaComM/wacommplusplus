# WaComM++

WaComM++ (Water quality COMmunity Model in C++) is a C++17 Lagrangian transport and diffusion framework for marine pollutants and drifting surface objects. Gridded Eulerian ocean forcing drives compact, independently advected particles whose state includes position, emission time, age, health, a stable 64-bit identity, and restart-safe drift-object metadata.

The current release reads local or remote ROMS, NEMO, HYCOM, and native WACOMM products through direction-neutral adapters. HTTP, HTTPS, and DAP4 locations use the linked NetCDF transport. Forcing is opened lazily as a bounded current/adjacent pair, and the adjacent normalized window is reused on the next iteration. It supports deterministic forward and backward traversal, configurable stochastic forward diffusion, an explicitly labelled stochastic backward ensemble mode, restart input/output, OpenMP, MPI, FlexMPI/EMPI, OpenACC, and optional CUDA. Unsupported URI schemes, transport capabilities, and adapter layouts are rejected rather than silently interpreted as another source or grid.

Surface-object tracking augments the ambient current with an empirically parameterized leeway velocity resolved into downwind and crosswind components relative to 10 m wind, plus optional surface Stokes drift. Generic weather and wave interfaces keep transport physics independent of forcing products; the initial concrete adapters read WRF wind and WAVEWATCH III Stokes velocity. The restart-stable object catalog covers people in several immersion states, liferafts, person-powered and small craft, vessels, containers, debris, drums, mines, refugee rafts, and medical waste. Its left and right crosswind regressions remain distinct where the source data are asymmetric. A separate process catalog documents the configuration, units, stochastic character, and state requirements of the implemented current, leeway, Stokes, diffusion, settling/rise, decay, wind-error, and jibing operators. Opt-in ensembles remain reproducible and passive transport remains the default. Tracking direction remains solver policy; stochastic backward output is a candidate-origin ensemble rather than an exact inverse realization.

Environmental adapters validate declared velocity and coordinate units and normalize supported CF numeric time coordinates to the WaComM physical epoch. Ambiguous units, unsupported calendars, unequal grids, and inconsistent timestamps are rejected before particle integration. Dynamic WRF wind and WW3 Stokes components use the same normalized-grid space/time interpolation in CPU and CUDA particle execution.

Environmental grids remain exact-match by default. Explicit geographic options interpolate Earth-relative WRF or WW3 vectors from rectilinear or valid curvilinear grids without extrapolation, with deterministic indexed curvilinear lookup. Optional `USE_PROJ=ON` adds declared-CRS transformation for rectilinear or valid curvilinear projected grids with metric `x/y` coordinates. Separate first- and limited second-order conservative operators remap rectilinear or convex-curvilinear geographic cell averages, including explicit fractional active area; they are not selectable for point-valued wind or Stokes velocity. Coordinate transformation never substitutes for vector rotation.

Backward deterministic tracking reverses forcing traversal and resolved/terminal motion and suppresses normal forward sources. It can identify candidate prior locations under the supplied circulation and model assumptions; stochastic backward tracking is not a unique inverse trajectory.

A read-only [trajectory diagnostic workflow](docs/trajectory-diagnostics.md) joins stable particle identities across output snapshots, computes explicitly descriptive ensemble-spread statistics, and creates a self-documenting SVG map with projection, extent, time, provenance, and uncertainty semantics. It does not infer probability contours or modify solver physics.

## Quick start

Install CMake 3.20+, a C++17 compiler, NetCDF C/C++ with DAP2 and DAP4 enabled, log4cplus, nlohmann-json, pkg-config, and any requested backend. Then run:

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

Options include `USE_OMP`, `USE_MPI`, `USE_EMPI`, `USE_OPENACC`, `USE_CUDA`, `USE_PROJ`, `DEBUG`, `BUILD_APPLICATION`, `BUILD_TESTING`, and `BUILD_REMOTE_INTEGRATION_TESTS`. MPI and EMPI are mutually exclusive. CUDA is unavailable on modern macOS; PROJ is optional and required only for projected environmental grids. The network integration test is opt-in so the default suite remains deterministic offline.

## Reproducible operation

Set `physics.random_seed` explicitly and archive the complete configuration, Git revision, forcing and restart checksums, compiler/dependency versions, platform, parallel settings, tolerances, test output, and result checksums. Production random behavior does not use wall-clock time. Examples, documentation, tests, source implementation, and physical definitions are versioned together.

## Documentation and examples

Start at the [documentation index](docs/README.md) for the [model](docs/model.md), [build guide](docs/build.md), [configuration](docs/configuration.md), [adapters](docs/adapters.md), [OpenDrift comparison and roadmap](docs/opendrift-comparison.md), [backtracking](docs/backtracking.md), [restart](docs/restart.md), [testing](docs/testing.md), [parallelism](docs/parallelism.md), [supported platforms](docs/supported-platforms.md), [reproducibility](docs/reproducibility.md), and [trajectory diagnostics](docs/trajectory-diagnostics.md). The [examples index](examples/README.md) covers every checked-in run configuration and source artifact, including forward/backward ROMS, NEMO, HYCOM, and native WACOMM workflows.

The [surface-drift guide](docs/sar-drift.md) defines the object-relative velocity model, coefficient provenance, units, ensemble assumptions, direction semantics, restart behavior, and supported object classes; the [catalog reference](docs/catalogs.md) lists every object and implemented process. Its paired catalog-mean [forward](examples/sar-person-forward.md) and [backward](examples/sar-person-backward.md) scenarios provide uniform-wind references, while the [kayak example](examples/sar-kayak-forward.md) exercises an asymmetric catalog entry. The ensemble and coupled-forcing examples are indexed in [examples](examples/README.md).

## References

Montella, R., et al. (2023), “A highly scalable high-performance Lagrangian transport and diffusion model for marine pollutants assessment,” *31st Euromicro International Conference on Parallel, Distributed and Network-Based Processing*, 17–26. Earlier WaComM publications and ocean-model references are listed in [references](docs/references.md).

Scientific use of surface-object drift should additionally cite the peer-reviewed leeway methodology and object-specific studies identified in [references](docs/references.md). Model output is conditional on forcing, parameterization, resolution, boundary treatment, and numerical configuration; it is not an observation or a unique reconstruction of an unobserved trajectory.

## License

WaComM++ is distributed under the terms in [LICENSE](LICENSE).
