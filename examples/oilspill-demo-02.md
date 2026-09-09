# Gaiola oil-spill polygon

## Scientific objective

`oilspill-demo-02.json` initializes 10,000 surface particles around Gaiola for a hypothetical near-shore oil-spill transport demonstration.

## Prerequisites and input

Use sufficiently resolved Gulf of Naples forcing and a configuration whose source reader accepts GeoJSON polygon releases. The polygon declares zero-metre depth and particle count but relies on the run configuration for timing.

## Configuration and exact use

Set `sources.active:true` and `sources.sources_file:"examples/oilspill-demo-02.json"` in a complete configuration, then run:

```bash
./build/wacommplusplus path/to/configuration.json
```

## Expected behavior and validation

Particles should initialize within the polygon on supported wet cells. Overlay the initial cloud on mask and bathymetry, check the exact count, examine coastline closures, and repeat with the same configured seed when random source placement is enabled.

## Limitations, interpretation, and reproducibility

Near-shore behavior is sensitive to grid resolution and closure settings. The scenario contains no oil weathering, mass, windage, or observations and cannot support impact claims by itself. Archive source/configuration and forcing checksums, revision, seed, compiler, dependencies, backend layout, tolerances, tests, and outputs. See [adapters](../docs/adapters.md) and [reproducibility](../docs/reproducibility.md).


## References

- Dagestad, K.-F., Röhrs, J., Breivik, Ø., and Ådlandsvik, B. (2018). OpenDrift v1.0: a generic framework for trajectory modelling. *Geoscientific Model Development*, 11, 1405–1420. [doi:10.5194/gmd-11-1405-2018](https://doi.org/10.5194/gmd-11-1405-2018).
- Montella, R., et al. (2023). A highly scalable high-performance Lagrangian transport and diffusion model for marine pollutants assessment. *Proceedings of PDP 2023*, 17–26. [doi:10.1109/PDP59025.2023.00012](https://doi.org/10.1109/PDP59025.2023.00012).
