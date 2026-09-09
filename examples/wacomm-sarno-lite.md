# Sarno River lite scenario

## Scientific objective

This small stochastic scenario follows a surface release near the Sarno River mouth over three hourly native WACOMM files. It is intended as a quick coastal-emission exercise.

## Prerequisites and fields

Build with NetCDF C++4 and place the three listed native files below `processed/`. The source is `sources-sarno_river.json`. Confirm that the point is wet and that the input contains the native grid, sigma, time, velocity, sea-level and diffusivity variables.

## Configuration and command

The configuration enables random transport with seed 5489, forward tracking, mask output, surface reflection, bottom constraint, and horizontal kill. Run:

```bash
./build/wacommplusplus examples/wacomm-sarno-lite.json
```

## Expected behavior and validation

Particles emit continuously at the declared source rate and stochastic results repeat for the same seed. Check counts, wet-cell placement, closure events and output times. Run `ctest --test-dir build -R "native_adapter|particle_physical_interval|numerical_helpers" --output-on-failure`, rerun unchanged, and compare checksums.

## Limitations, interpretation, and reproducibility

The particle rate is not a contaminant mass flux, and a two-hour window cannot establish long-term exposure. Random output is an ensemble sample. Archive revision, resolved configuration, source/forcing checksums, seed, compiler/dependencies, parallel settings, tolerances, tests, and results.


## References

- Dagestad, K.-F., Röhrs, J., Breivik, Ø., and Ådlandsvik, B. (2018). OpenDrift v1.0: a generic framework for trajectory modelling. *Geoscientific Model Development*, 11, 1405–1420. [doi:10.5194/gmd-11-1405-2018](https://doi.org/10.5194/gmd-11-1405-2018).
- Montella, R., et al. (2023). A highly scalable high-performance Lagrangian transport and diffusion model for marine pollutants assessment. *Proceedings of PDP 2023*, 17–26. [doi:10.1109/PDP59025.2023.00012](https://doi.org/10.1109/PDP59025.2023.00012).
