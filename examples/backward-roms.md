# Deterministic backward ROMS example

This example estimates candidate prior positions for known endpoints under a ROMS circulation product. It requires the same ROMS fields described by the forward example and an endpoint NetCDF restart. Sources are disabled because ordinary release sources do not emit during backward tracking.

Build the application, replace paths, and run `./build/wacommplusplus examples/backward-roms.json`. Files and records are traversed newest to oldest and deterministic velocity and terminal motion are reversed. Validate with a diffusion-free forward/backward round trip away from boundaries.

This is conditional trajectory analysis, not proof of an exact source. Resolution, forcing errors, irreversible boundaries, and missing physics limit inference. Archive the complete reproducibility metadata described in [reproducibility](../docs/reproducibility.md) and consult [backtracking](../docs/backtracking.md).
