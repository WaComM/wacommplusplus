# Deterministic forward NEMO example

## Scientific objective

Track where particles released from known sources travel under a NEMO circulation product. This deterministic run addresses transport or exposure questions, not unresolved uncertainty.

## Prerequisites and required fields

Build with NetCDF C++4 and provide `nemo.nc` plus `sources.json`. The adapter accepts `time_counter/time`, `nav_lon/longitude/lon`, `nav_lat/latitude/lat`, `deptht/depth`, `uo/vozocrtx/u`, and `vo/vomecrty/v`. U and V must share horizontal dimensions. Optional SSH, mask, W, and AKT aliases and zero fallbacks are listed in [adapters](../docs/adapters.md).

## Configuration and run

The JSON selects `NEMO`, enables sources, disables stochastic motion, records seed `5489`, limits particle steps to 30 s, and traverses forcing forward. Replace paths, then run `cmake -S . -B build && cmake --build build` and `./build/wacommplusplus examples/forward-nemo.json`.

## Expected behavior and validation

Ocean time must increase strictly. Particles appear only at emission times and move with temporally interpolated velocity. Repeat the case and compare counts and checksums; inspect bounds and compare a constant-flow fixture with analytical displacement. Run `ctest --test-dir build -R structured_grid_adapters --output-on-failure`.

## Limitations, interpretation, and reproducibility

Unsupported NEMO staggering fails explicitly. Missing W or AKT removes resolved vertical velocity or vertical diffusion. Interpret trajectories conditional on forcing, sources, and closures. Archive the revision, configuration, input checksums, toolchain, dependencies, platform/backend settings, tolerances, tests, and output checksums.
