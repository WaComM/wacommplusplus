# Native-WACOMM search-and-rescue scenario

## Scientific objective

This historical forward scenario follows a surface search-and-rescue ensemble from a known position using hourly native WACOMM forcing from 2021-07-01 09:00 through 2021-07-03 00:00 UTC.

## Prerequisites and fields

Build the application with NetCDF C++4. Supply the listed files below `processed/` with native `ocean_time`, sigma coordinates, grid, mask, bathymetry, sea level, velocity, and diffusivity fields. The checked-in `sources-sar.json` provides the release. Verify forcing timestamps and spatial coverage before use.

## Configuration and command

The file selects `WaComM`, forward tracking, an explicit seed, source emission, hourly forcing, text history, and reflection/kill closures. From the repository root run:

```bash
./build/wacommplusplus examples/wacomm-native-sar.json
```

## Expected behavior and validation

Particles emit at 2021-07-01 09:00 UTC and traverse forcing chronologically. Check accepted-source logs, time bounds, particle counts, closure events, and output/history timestamps. Run `ctest --test-dir build -R "native_adapter|particle_physical_interval" --output-on-failure` and compare deterministic checksums with a serial reference.

## Limitations, interpretation, and reproducibility

This scenario omits object leeway and observational uncertainty and is not operational rescue guidance. External forcing is not bundled. Results are candidate drift conditional on forcing and model settings. Archive revision, resolved configuration, source/forcing checksums, seed, compiler/dependencies, CMake options, platform/backend layout, tolerances, tests, and outputs.
