# WaComM++

WaComM++ (Water quality COMmunity Model in C++) is a C++17 Lagrangian transport and diffusion model for marine pollutant assessment. Gridded Eulerian ocean forcing drives independent particles whose state includes position, emission time, age, health, and a stable 64-bit identity.

The current release reads ROMS, NEMO, HYCOM, and native WACOMM products through direction-neutral adapters. It supports deterministic forward and backward traversal, configurable stochastic forward diffusion, an explicitly labeled stochastic backward ensemble mode, restart input/output, OpenMP, MPI, FlexMPI/EMPI, OpenACC, and optional CUDA. Unsupported adapter layouts are rejected rather than silently interpreted as another grid.

Backward deterministic tracking reverses forcing traversal and resolved/terminal motion and suppresses normal forward sources. It can identify candidate prior locations under the supplied circulation and model assumptions; stochastic backward tracking is not a unique inverse trajectory.

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

Options include `USE_OMP`, `USE_MPI`, `USE_EMPI`, `USE_OPENACC`, `USE_CUDA`, `DEBUG`, `BUILD_APPLICATION`, and `BUILD_TESTING`. MPI and EMPI are mutually exclusive. CUDA is optional and unavailable on modern macOS.

## Reproducible operation

Set `physics.random_seed` explicitly and archive the complete configuration, Git revision, forcing and restart checksums, compiler/dependency versions, platform, parallel settings, tolerances, test output, and result checksums. Production random behavior does not use wall-clock time. Examples, documentation, tests, source implementation, and physical definitions are versioned together.

## Documentation and examples

Start at the [documentation index](docs/README.md) for the [model](docs/model.md), [build guide](docs/build.md), [configuration](docs/configuration.md), [adapters](docs/adapters.md), [backtracking](docs/backtracking.md), [restart](docs/restart.md), [testing](docs/testing.md), [parallelism](docs/parallelism.md), and [reproducibility](docs/reproducibility.md). The [examples index](examples/README.md) describes release expectations; documented deterministic forward and backward ROMS configurations are included.

## Citation

Montella, R., et al. (2023), “A highly scalable high-performance Lagrangian transport and diffusion model for marine pollutants assessment,” *31st Euromicro International Conference on Parallel, Distributed and Network-Based Processing*, 17–26. Earlier WaComM publications and ocean-model references are listed in [references](docs/references.md).

## License

WaComM++ is distributed under the terms in [LICENSE](LICENSE).
