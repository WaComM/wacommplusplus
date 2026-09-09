# Deterministic backward NEMO example

## Scientific objective

Estimate candidate origins of known endpoint particles by reversing deterministic transport through NEMO forcing. This is a reversibility experiment, not proof of a unique source.

## Prerequisites and required fields

Build with NetCDF C++4, provide chronological `nemo.nc`, and provide versioned endpoint state `endpoint.nc`. Required aliases and the common-grid U/V restriction are documented in [adapters](../docs/adapters.md). Sources remain disabled.

## Configuration and run

The JSON selects `direction:backward`, `backward_diffusion:none`, deterministic physics, and restart input. Replace paths, run `cmake -S . -B build && cmake --build build`, then `./build/wacommplusplus examples/backward-nemo.json`.

## Expected behavior and validation

The adapter preserves chronological time and velocity sign; the solver visits records newest to oldest and reverses deterministic displacement. Verify restart direction metadata, identities, decreasing physical output time, and bounds. With non-reversible processes disabled, compare a forward-then-backward fixture to its start and run the adapter and restart CTests.

## Limitations, interpretation, and reproducibility

Closures, decay, and numerical error are not necessarily invertible, and missing fields remove modeled processes. Report candidate origins conditional on the forcing. Archive revision, configuration, endpoint/forcing checksums, seed, toolchain, dependencies, platform/backend settings, tolerances, tests, and outputs.
