# Deterministic forward ROMS example

This example asks where particles released from known sources travel under a ROMS circulation product. It requires a processed ROMS NetCDF file with grid masks and coordinates, bathymetry, `ocean_time`, sigma coordinates, `zeta`, staggered `u/v`, `w`, and `AKt`, plus a compatible source file.

Build with `cmake -S . -B build && cmake --build build`, replace the forcing and source paths, then run `./build/wacommplusplus examples/forward-roms.json`. Particle count should increase at source emission times and deterministic trajectories should repeat bit-for-bit on the same build. Validate domain bounds, output times, count preservation, and a known constant-flow displacement.

ROMS resolution, forcing quality, closures, interpolation, and omitted processes limit interpretation. Archive the Git revision, configuration, input checksums, compiler/dependencies, platform, backend settings, test results, tolerances, and output checksums. See [model](../docs/model.md), [adapters](../docs/adapters.md), and [testing](../docs/testing.md).
