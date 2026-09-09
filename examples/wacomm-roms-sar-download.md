# ROMS search-and-rescue download scenario

## Scientific objective

This historical forward scenario follows a known surface search-and-rescue release with hourly ROMS forcing from 2021-07-01 09:00 through 2021-07-03 00:00 UTC.

## Prerequisites and fields

Build with NetCDF C++4 and ensure the configured OPeNDAP endpoint is reachable. ROMS files must contain chronological `ocean_time`, rho-grid coordinates/mask/bathymetry, sigma metadata, zeta, staggered U/V, and configured W/AKT inputs. The scenario uses `sources-sar.json`.

## Configuration and command

Review the remote URLs first; historical services can move. The configuration selects `ROMS`, dry input caching, forward tracking, a fixed seed, and constraint/kill/reflection closures. Run:

```bash
./build/wacommplusplus examples/wacomm-roms-sar-download.json
```

## Expected behavior and validation

The adapter normalizes ROMS fields without changing velocity signs or time direction. Check downloads, source acceptance, chronological boundaries, counts, and closure events. Run `ctest --test-dir build -R "roms_adapter|particle_physical_interval" --output-on-failure` and compare with a cached-input rerun.

## Limitations, interpretation, and reproducibility

Remote availability and content are outside the repository; download and checksum inputs for a reproducible run. The model omits object-specific leeway and cannot establish a unique trajectory. Archive revision, configuration, source/forcing checksums, seed, toolchain, backend settings, tolerances, tests, and outputs.
