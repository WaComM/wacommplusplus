# Native-WACOMM webinar use case

## Scientific objective

This historical webinar scenario demonstrates high-density stochastic drift with native WACOMM forcing and NetCDF restart history.

## Prerequisites and fields

Build with NetCDF C++4, provide the three named 2019 native files under `processed/`, and ensure their physical timestamps overlap the simulation window. The source requests 250,000 particles per hour; perform a reduced-count smoke run before allocating a production job.

## Configuration and command

The file selects native `WaComM`, forward stochastic transport with seed 5489, random source placement, three closure modes, and two-hour restart output. Run:

```bash
./build/wacommplusplus examples/webinar-native-usecase.json
```

## Expected behavior and validation

The configured source maps to a wet cell and repeat runs with identical seed/input produce equivalent state. Check forcing and simulation timestamps, counts, restart metadata, and output checksums. Run `ctest --test-dir build -R "native_adapter|restart_format|concentration" --output-on-failure`.

## Limitations, interpretation, and reproducibility

External data is not bundled, and the sampling density is not a physical flux. Confirm the actual `ocean_time` values against the declared 08:00-10:00 UTC simulation before scientific use. Archive resolved configuration, all inputs/checksums, revision, seed, toolchain, backend layout, tolerances, tests, restarts, and outputs.
