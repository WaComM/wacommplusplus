# Deterministic forward HYCOM example

## Scientific objective

Track released particles forward using a HYCOM/GOFS product and quantify resolved transport under a deterministic configuration.

## Prerequisites and required fields

Build with NetCDF C++4 and provide `hycom.nc` plus `sources.json`. Required variables are `time/MT`, `lon/longitude`, `lat/latitude`, `depth`, `water_u/u`, and `water_v/v`. SSH, bathymetry, W, and diffusivity aliases are optional as described in [adapters](../docs/adapters.md).

## Configuration and run

The JSON selects `HYCOM`, forward deterministic tracking, source emission, seed `5489`, and maximum 30 s steps. Replace paths, run `cmake -S . -B build && cmake --build build`, then `./build/wacommplusplus examples/forward-hycom.json`.

## Expected behavior and validation

The adapter normalizes longitudes greater than 180 degrees and maps positive-down depth onto bottom-to-surface logical levels without changing velocity sign. Repeat the run for identical checksums, validate counts and bounds, and run `ctest --test-dir build -R structured_grid_adapters --output-on-failure`.

## Limitations, interpretation, and reproducibility

When W or AKT is absent, the zero fallback excludes that process. Product resolution and coordinate assumptions bound interpretation. Archive the revision, configuration, input checksums, toolchain, dependencies, platform/backend settings, tolerances, tests, and output checksums.
