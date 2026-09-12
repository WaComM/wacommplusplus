# ROMS search-and-rescue download scenario

## Scientific objective

This historical forward scenario follows a known surface search-and-rescue release with hourly ROMS forcing from 2021-07-01 09:00 through 2021-07-03 00:00 UTC.

Despite its historical name, this configuration is passive because it does not select `drift.model=leeway`. It represents current-driven tracer transport rather than object-specific wind leeway.

## Prerequisites and fields

Build with NetCDF C++4 providing DAP2/DAP4 and ensure the configured OPeNDAP endpoint is reachable. ROMS files must contain chronological `ocean_time`, rho-grid coordinates/mask/bathymetry, sigma metadata, zeta, staggered U/V, and configured W/AKT inputs. The scenario uses `sources-sar.json`.

This is the documented remote/lazy-access example. Each relative name is resolved against the HTTP `io.base_path`. WaComM++ opens only the current dataset and the adjacent dataset required for boundary interpolation, reuses that adjacent adapter in the following window, and fails if the endpoint or required metadata is unavailable. It does not mirror the archive or substitute stale values.

## Configuration and command

Review the remote URLs first; historical services can move. The configuration selects `ROMS`, a dry adapter-processing run, forward tracking, a fixed seed, and constraint/kill/reflection closures. Run:

```bash
./build/wacommplusplus examples/wacomm-roms-sar-download/wacomm-roms-sar-download.json
```

## Expected behavior and validation

The adapter normalizes ROMS fields without changing velocity signs or time direction. Check source acceptance, chronological boundaries, counts, and closure events. Run `ctest --test-dir build -R "roms_adapter|particle_physical_interval" --output-on-failure`; with `BUILD_REMOTE_INTEGRATION_TESTS=ON`, run `ctest --test-dir build -R remote_netcdf_integration --output-on-failure`. Compare the remote result with a checksum-pinned local rerun.

## Limitations, interpretation, and reproducibility

Remote availability and content are outside the repository; download and checksum inputs for a reproducible run. The model omits object-specific leeway and cannot establish a unique trajectory. Archive revision, configuration, source/forcing checksums, seed, toolchain, backend settings, tolerances, tests, and outputs.


## References

- Dagestad, K.-F., Röhrs, J., Breivik, Ø., and Ådlandsvik, B. (2018). OpenDrift v1.0: a generic framework for trajectory modelling. *Geoscientific Model Development*, 11, 1405–1420. [doi:10.5194/gmd-11-1405-2018](https://doi.org/10.5194/gmd-11-1405-2018).
- Montella, R., et al. (2023). A highly scalable high-performance Lagrangian transport and diffusion model for marine pollutants assessment. *Proceedings of PDP 2023*, 17–26. [doi:10.1109/PDP59025.2023.00012](https://doi.org/10.1109/PDP59025.2023.00012).
