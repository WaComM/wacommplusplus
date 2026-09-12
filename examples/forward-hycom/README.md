# Deterministic forward HYCOM example

## Scientific objective

Track released particles forward using a HYCOM/GOFS product and quantify resolved transport under a deterministic configuration.

## Prerequisites and required fields

Build with NetCDF C++4 and provide `hycom.nc` plus `sources.json`. Required variables are `time/MT`, `lon/longitude`, `lat/latitude`, `depth`, `water_u/u`, and `water_v/v`. SSH, bathymetry, W, and diffusivity aliases are optional as described in [adapters](../../docs/adapters.md).

## Configuration and run

The JSON selects `HYCOM`, forward deterministic tracking, source emission, seed `5489`, and maximum 30 s steps. Replace paths, run `cmake -S . -B build && cmake --build build`, then `./build/wacommplusplus examples/forward-hycom/forward-hycom.json`.

## Expected behavior and validation

The adapter normalizes longitudes greater than 180 degrees and maps positive-down depth onto bottom-to-surface logical levels without changing velocity sign. Repeat the run for identical checksums, validate counts and bounds, and run `ctest --test-dir build -R structured_grid_adapters --output-on-failure`.

## Limitations, interpretation, and reproducibility

When W or AKT is absent, the zero fallback excludes that process. Product resolution and coordinate assumptions bound interpretation. Archive the revision, configuration, input checksums, toolchain, dependencies, platform/backend settings, tolerances, tests, and output checksums.


## References

- Dagestad, K.-F., Röhrs, J., Breivik, Ø., and Ådlandsvik, B. (2018). OpenDrift v1.0: a generic framework for trajectory modelling. *Geoscientific Model Development*, 11, 1405–1420. [doi:10.5194/gmd-11-1405-2018](https://doi.org/10.5194/gmd-11-1405-2018).
- Montella, R., et al. (2023). A highly scalable high-performance Lagrangian transport and diffusion model for marine pollutants assessment. *Proceedings of PDP 2023*, 17–26. [doi:10.1109/PDP59025.2023.00012](https://doi.org/10.1109/PDP59025.2023.00012).
