# Native-WACOMM search-and-rescue scenario

## Scientific objective

This historical forward scenario follows a surface search-and-rescue ensemble from a known position using hourly native WACOMM forcing from 2021-07-01 09:00 through 2021-07-03 00:00 UTC.

Despite its historical name, this configuration is passive because it does not select `drift.model=leeway`. It represents current-driven tracer transport rather than object-specific wind leeway.

## Prerequisites and fields

Build the application with NetCDF C++4. Supply the listed files below `processed/` with native `ocean_time`, sigma coordinates, grid, mask, bathymetry, sea level, velocity, and diffusivity fields. The checked-in `sources-sar.json` provides the release. Verify forcing timestamps and spatial coverage before use.

## Configuration and command

The file selects `WaComM`, forward tracking, an explicit seed, source emission, hourly forcing, text history, and reflection/kill closures. From the repository root run:

```bash
./build/wacommplusplus examples/wacomm-native-sar.json
```

## Expected behavior and validation

Particles emit at 2021-07-01 09:00 UTC and traverse forcing chronologically. Check accepted-source logs, time bounds, particle counts, closure events, and output/history timestamps. Run `ctest --test-dir build -R "native_adapter|particle_physical_interval" --output-on-failure` and compare deterministic checksums with a serial reference.

## Limitations, interpretation, and reproducibility

This scenario omits object leeway and observational uncertainty and is not operational rescue guidance. External forcing is not bundled. Results are candidate drift conditional on forcing and model settings. Archive revision, resolved configuration, source/forcing checksums, seed, compiler/dependencies, CMake options, platform/backend layout, tolerances, tests, and outputs.


The historical `WaComM` selector used here and uppercase `WACOMM` select the same native adapter. When replaying saved windows with an already-present adjacent boundary, the shared loader reuses equal-time records only if all stored dynamic fields agree exactly; conflicting overlaps fail. See [native replay](../docs/adapters.md#reusing-saved-native-boundary-records) and the [regenerated-forcing scaling workflow](wacomm-sarno-lite.md#mpiopenmp-strong-scaling).

## References

- Dagestad, K.-F., Röhrs, J., Breivik, Ø., and Ådlandsvik, B. (2018). OpenDrift v1.0: a generic framework for trajectory modelling. *Geoscientific Model Development*, 11, 1405–1420. [doi:10.5194/gmd-11-1405-2018](https://doi.org/10.5194/gmd-11-1405-2018).
- Montella, R., et al. (2023). A highly scalable high-performance Lagrangian transport and diffusion model for marine pollutants assessment. *Proceedings of PDP 2023*, 17–26. [doi:10.1109/PDP59025.2023.00012](https://doi.org/10.1109/PDP59025.2023.00012).
