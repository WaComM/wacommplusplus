# Deterministic backward ROMS example

## Scientific objective

Estimate candidate prior positions for known endpoint particles under ROMS circulation. This tests deterministic reverse transport and supports conditional source-attribution analysis; it does not establish a unique origin.

## Prerequisites and required fields

Provide the same chronological ROMS fields required by the forward example and a versioned `endpoint.nc` containing physical checkpoint time, backward-compatible direction, model name, seed, and 64-bit identities. Sources are disabled because ordinary forward release sources do not emit during backtracking.

## Configuration and run

`backward-roms.json` selects the ROMS adapter, deterministic motion, `direction:backward`, `backward_diffusion:none`, and endpoint restart input. Replace paths, build with `cmake -S . -B build && cmake --build build`, then run `./build/wacommplusplus examples/backward-roms.json`.

## Expected behavior and validation

The adapter leaves timestamps and velocity signs unchanged. The solver traverses files and records newest to oldest and reverses resolved and terminal velocity displacement. Verify physical output time, identities, bounds, and restart metadata. Away from closures, disable stochastic motion and other irreversible effects, integrate a fixture forward and then backward, and compare with its initial state. Run the ROMS, particle-interval, and restart CTests.

## Limitations and interpretation

Finite resolution, forcing error, decay, sources, and boundary interactions can prevent exact reversibility. Report output as candidate origins conditional on circulation and model assumptions. Stochastic reverse-time interpretation is separately documented in [backtracking](../docs/backtracking.md).

## Reproducibility

Archive revision, configuration, forcing/restart checksums, seed, compiler/dependencies, CMake options, platform/backend settings, tolerances, test results, and output checksums. See [reproducibility](../docs/reproducibility.md).
