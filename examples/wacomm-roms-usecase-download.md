# ROMS Campania download use case

## Scientific objective

This historical scenario evaluates forward transport from multiple Campania coastal release points using hourly ROMS forcing from 2020-11-30 through 2020-12-01.

## Prerequisites and fields

Build with NetCDF C++4 and verify access to the configured historical OPeNDAP service. Inputs must satisfy the ROMS adapter contract, including chronological time, rho-grid geometry, sigma metadata, zeta and staggered velocity fields. Verify `sources-campania_region.json` points against the actual grid.

## Configuration and command

The scenario selects `ROMS`, dry input caching, forward tracking, a fixed seed, and constraint/kill/reflection closures. Replace an unavailable endpoint before running:

```bash
./build/wacommplusplus examples/wacomm-roms-usecase-download.json
```

## Expected behavior and validation

Files are traversed oldest-to-newest and adapters do not reverse or negate fields. Inspect input caching, source mapping, particle totals, interval boundaries, and outputs. Run `ctest --test-dir build -R "roms_adapter|particle_physical_interval|concentration" --output-on-failure` and compare cached reruns.

## Limitations, interpretation, and reproducibility

The external archive and source rates are not controlled or calibrated by this repository. Results are conditional on the forcing version, resolution and model closures. Preserve downloaded forcing and checksums, source/configuration checksums, revision, seed, toolchain, backend layout, tolerances, tests, and output checksums.


## References

- Dagestad, K.-F., Röhrs, J., Breivik, Ø., and Ådlandsvik, B. (2018). OpenDrift v1.0: a generic framework for trajectory modelling. *Geoscientific Model Development*, 11, 1405–1420. [doi:10.5194/gmd-11-1405-2018](https://doi.org/10.5194/gmd-11-1405-2018).
- Montella, R., et al. (2023). A highly scalable high-performance Lagrangian transport and diffusion model for marine pollutants assessment. *Proceedings of PDP 2023*, 17–26. [doi:10.1109/PDP59025.2023.00012](https://doi.org/10.1109/PDP59025.2023.00012).
