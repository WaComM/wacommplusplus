# Stochastic forward example

## Scientific objective

Estimate the dispersion envelope of particles released into a resolved ROMS circulation field. This is an ensemble experiment; an individual random trajectory is not a deterministic forecast.

## Prerequisites and input

Build the application and provide `forcing.nc` with the ROMS fields documented in [adapters](../docs/adapters.md), plus a compatible `sources.json`. The configuration enables diffusion with seed `5489`, standard deviation `3.46 m` per nominal 30 s step, and a maximum integration step of 30 s.

## Run and expected behavior

Configure and run with `cmake -S . -B build && cmake --build build` followed by `./build/wacommplusplus examples/stochastic-forward.json`. Repeating the run with the same inputs, executable, and seed must reproduce particle state; changing the seed should change stochastic displacement without changing emissions.

## Validation, limitations, and interpretation

Compare two identical-seed output checksums, then repeat with a different seed. Check particle counts and domain bounds. The result is conditional on forcing resolution, closure choices, diffusion parameterization, and source assumptions; it represents possible dispersion, not observational uncertainty.

## Reproducibility

Archive the Git revision, complete configuration, forcing/source checksums, seed, compiler and dependencies, CMake options, platform, parallel settings, tolerances, tests, and output checksums. Related material: [model](../docs/model.md), [reproducibility](../docs/reproducibility.md), and `numerical_helpers_test`.
