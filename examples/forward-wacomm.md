# Deterministic forward native-WACOMM example

## Scientific objective

Run transport from known releases using an already normalized native WACOMM forcing file. This isolates solver behavior from external-model grid conversion.

## Prerequisites and required fields

Build with NetCDF C++4 and provide `wacomm.nc` and `sources.json`. Native input contains `ocean_time`, `s_rho`, `s_w`, `lat_rho`, `lon_rho`, `mask_rho`, `h`, `zeta`, `u`, `v`, `w`, and `akt` on the common rho-point grid. The current `xi_rho` dimension and historical `eta_xi` spelling are accepted.

## Configuration and run

The JSON selects the `WACOMM` adapter, deterministic forward tracking, source emission, and a maximum 30 s step. Replace paths, run `cmake -S . -B build && cmake --build build`, then `./build/wacommplusplus examples/forward-wacomm.json`.

## Expected behavior and validation

The loader preserves chronological time, signs, normalized levels, and field values. Validate emissions, bounds, count preservation, and deterministic checksums. Run `ctest --test-dir build -R "native_adapter|particle_physical_interval" --output-on-failure` to check native serialization and interval integration.

## Limitations, interpretation, and reproducibility

Native format avoids adapter interpolation but not forcing-resolution, closure, or modeled-process limitations. Archive revision, complete configuration, forcing/source checksums, toolchain, dependencies, CMake/backend settings, platform, tolerances, tests, and output checksums.


## References

- Dagestad, K.-F., Röhrs, J., Breivik, Ø., and Ådlandsvik, B. (2018). OpenDrift v1.0: a generic framework for trajectory modelling. *Geoscientific Model Development*, 11, 1405–1420. [doi:10.5194/gmd-11-1405-2018](https://doi.org/10.5194/gmd-11-1405-2018).
- Montella, R., et al. (2023). A highly scalable high-performance Lagrangian transport and diffusion model for marine pollutants assessment. *Proceedings of PDP 2023*, 17–26. [doi:10.1109/PDP59025.2023.00012](https://doi.org/10.1109/PDP59025.2023.00012).
