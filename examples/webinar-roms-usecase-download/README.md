# ROMS webinar download use case

## Scientific objective

This historical scenario demonstrates forward drift using hourly ROMS forcing on 2019-04-01 and the webinar surface source.

## Prerequisites and fields

Build with NetCDF C++4 providing DAP2/DAP4 and verify the historical OPeNDAP endpoint. Inputs require chronological ROMS time, rho-grid geometry, mask and bathymetry, sigma metadata, zeta, staggered U/V, and the W/AKT fields expected by the adapter. The checked-in webinar source has a large sampling rate.

## Configuration and command

The configuration selects `ROMS`, a dry adapter-processing run, forward tracking, seed 5489, and constraint/kill/reflection closures. Relative forcing names resolve against the configured HTTP `io.base_path`. WaComM++ opens the current and adjacent ROMS windows on demand, reuses the adjacent normalized adapter for the next interval, and releases the previous window. Review remote availability and run:

```bash
./build/wacommplusplus examples/webinar-roms-usecase-download/webinar-roms-usecase-download.json
```

## Expected behavior and validation

The adapter preserves physical signs and chronological data while the solver controls direction. Check source mapping, count preservation, interval timestamps and closure events. Run `ctest --test-dir build -R "roms_adapter|particle_physical_interval|concentration" --output-on-failure`. Configure `BUILD_REMOTE_INTEGRATION_TESTS=ON` and run `ctest --test-dir build -R remote_netcdf_integration --output-on-failure` to verify live OPeNDAP metadata and hyperslab transport independently of this historical endpoint.

## Limitations, interpretation, and reproducibility

The remote archive may change or disappear, and the scenario is demonstrative rather than calibrated. Download and checksum every file; record source/configuration checksums, revision, seed, compiler/dependencies, backend layout, tolerances, tests, and output checksums.


## References

- Dagestad, K.-F., Röhrs, J., Breivik, Ø., and Ådlandsvik, B. (2018). OpenDrift v1.0: a generic framework for trajectory modelling. *Geoscientific Model Development*, 11, 1405–1420. [doi:10.5194/gmd-11-1405-2018](https://doi.org/10.5194/gmd-11-1405-2018).
- Montella, R., et al. (2023). A highly scalable high-performance Lagrangian transport and diffusion model for marine pollutants assessment. *Proceedings of PDP 2023*, 17–26. [doi:10.1109/PDP59025.2023.00012](https://doi.org/10.1109/PDP59025.2023.00012).
