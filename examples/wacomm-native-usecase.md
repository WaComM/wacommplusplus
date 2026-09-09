# Native-WACOMM Campania use case

## Scientific objective

This historical deterministic scenario samples transport from multiple Campania coastal sources on 2020-11-30 using native WACOMM forcing.

## Prerequisites and fields

Build with NetCDF C++4 and provide the 24 listed hourly files beneath `processed/`. Each file must satisfy the native adapter contract. The scenario uses `sources-campania_region.json`; verify every physical point against the forcing mask because its legacy grid-index hints are product-specific.

## Configuration and command

The configuration selects `WaComM`, deterministic forward tracking, a fixed seed, source emission, and reflection/kill closures. Run:

```bash
./build/wacommplusplus examples/wacomm-native-usecase.json
```

## Expected behavior and validation

The solver traverses the hourly records in chronological physical time and emits from accepted wet points. Inspect rejections, counts, bounds and timestamps, then run `ctest --test-dir build -R "native_adapter|particle_physical_interval|concentration" --output-on-failure`. Compare serial and OpenMP output within declared tolerances.

## Limitations, interpretation, and reproducibility

The source rates are sampling rates rather than calibrated fluxes. External forcing is not distributed. Interpret output as transport conditional on forcing, grid resolution, closure and decay choices. Archive revision, complete configuration, source/forcing checksums, seed, toolchain, dependencies, backend settings, tolerances, tests, and output checksums.
