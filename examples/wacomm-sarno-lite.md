# Sarno River lite scenario

## Scientific objective

This small stochastic scenario follows a surface release near the Sarno River mouth over three hourly native WACOMM files. It is intended as a quick coastal-emission exercise.

## Prerequisites and fields

Build with NetCDF C++4 and place the three listed native files below `processed/`. The source is `sources-sarno_river.json`. Confirm that the point is wet and that the input contains the native grid, sigma, time, velocity, sea-level and diffusivity variables.

## Configuration and command

The configuration enables random transport with seed 5489, forward tracking, mask output, surface reflection, bottom constraint, and horizontal kill. Run:

```bash
./build/wacommplusplus examples/wacomm-sarno-lite.json
```

## Expected behavior and validation

Particles emit continuously at the declared source rate and stochastic results repeat for the same seed. Check counts, wet-cell placement, closure events and output times. Run `ctest --test-dir build -R "native_adapter|particle_physical_interval|numerical_helpers" --output-on-failure`, rerun unchanged, and compare checksums.

## Limitations, interpretation, and reproducibility

The particle rate is not a contaminant mass flux, and a two-hour window cannot establish long-term exposure. Random output is an ensemble sample. Archive revision, resolved configuration, source/forcing checksums, seed, compiler/dependencies, parallel settings, tolerances, tests, and results.
