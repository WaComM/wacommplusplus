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


## References

- Dagestad, K.-F., Röhrs, J., Breivik, Ø., and Ådlandsvik, B. (2018). OpenDrift v1.0: a generic framework for trajectory modelling. *Geoscientific Model Development*, 11, 1405–1420. [doi:10.5194/gmd-11-1405-2018](https://doi.org/10.5194/gmd-11-1405-2018).
- Montella, R., et al. (2023). A highly scalable high-performance Lagrangian transport and diffusion model for marine pollutants assessment. *Proceedings of PDP 2023*, 17–26. [doi:10.1109/PDP59025.2023.00012](https://doi.org/10.1109/PDP59025.2023.00012).
