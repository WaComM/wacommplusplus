# Deterministic forward ROMS example

## Scientific objective

Determine where particles released from known sources travel under a ROMS circulation product. The deterministic configuration is intended for resolved transport and exposure questions.

## Prerequisites and required fields

Build with NetCDF C++4 and provide `forcing.nc` plus `sources.json`. ROMS input requires `ocean_time`, `s_rho`, `s_w`, `mask_rho`, `mask_u`, `mask_v`, `lat_rho`, `lon_rho`, `lat_v`, `lon_u`, bathymetry `h`, `zeta`, staggered `u/v`, rho-grid `w`, and `AKt`. Records must be chronological and adjacent files must have compatible grids.

## Configuration and run

`forward-roms.json` selects the ROMS adapter, enables ordinary sources, disables stochastic displacement, records seed `5489`, uses `dti=30 s` as a maximum step, and selects forward tracking. Replace paths, then run `cmake -S . -B build && cmake --build build` and `./build/wacommplusplus examples/forward-roms.json`.

## Expected behavior and validation

The adapter averages the two available wet U faces along xi and V faces along eta onto each rho point, using the single available face at an edge. Dynamic fields interpolate between physical timestamps and the final shortened step lands on the boundary. Confirm emission counts, bounds, output times, and repeatable checksums. Run `ctest --test-dir build -R "roms_adapter|particle_physical_interval" --output-on-failure` and compare a constant-flow displacement analytically.

## Limitations and interpretation

Trajectories are conditional on ROMS resolution, forcing quality, source assumptions, closures, temporal/spatial interpolation, and processes represented by the configuration. A deterministic path does not quantify circulation or source uncertainty.

## Reproducibility

Archive the Git revision, full configuration, forcing/source manifests and checksums, compiler/dependencies, CMake options, platform and architecture, MPI/OpenMP/CUDA settings, numerical tolerances, test results, and output checksums. See [model](../docs/model.md), [adapters](../docs/adapters.md), and [testing](../docs/testing.md).
