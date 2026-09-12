# Capri–Punta Campanella oil-spill polygon

## Scientific objective

`oilspill-demo-01.json` initializes 10,000 surface particles inside a polygon between Capri and Punta Campanella for a hypothetical oil-spill transport demonstration.

## Prerequisites and input

Use forcing that covers the Gulf of Naples and a run configuration whose source reader accepts GeoJSON polygon releases. The polygon supplies particle count and zero-metre depth but no release timestamp; the enclosing configuration must define the time semantics.

## Configuration and exact use

Set `sources.active:true` and point `sources.sources_file` at this file in a complete configuration, then run:

```bash
./build/wacommplusplus path/to/configuration.json
```

## Expected behavior and validation

Initial particles should sample the polygon and valid wet cells. Plot the initialized cloud with the coastline, verify the total count, ensure no point lies outside supported interpolation cells, and compare repeated runs using an explicit `physics.random_seed` when random source placement is enabled.

## Limitations, interpretation, and reproducibility

This hypothetical release does not specify oil chemistry, weathering, windage, mass, or observations. Interpret it only as particle transport under the configured model. Archive the polygon, configuration, forcing checksums, revision, seed, toolchain, backend settings, tolerances, test results, and output checksums. See [configuration](../../docs/configuration.md) and [model](../../docs/model.md).


## References

- Dagestad, K.-F., Röhrs, J., Breivik, Ø., and Ådlandsvik, B. (2018). OpenDrift v1.0: a generic framework for trajectory modelling. *Geoscientific Model Development*, 11, 1405–1420. [doi:10.5194/gmd-11-1405-2018](https://doi.org/10.5194/gmd-11-1405-2018).
- Montella, R., et al. (2023). A highly scalable high-performance Lagrangian transport and diffusion model for marine pollutants assessment. *Proceedings of PDP 2023*, 17–26. [doi:10.1109/PDP59025.2023.00012](https://doi.org/10.1109/PDP59025.2023.00012).
