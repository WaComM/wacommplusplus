# Revised Campania regional point sources

## Scientific objective

`new-sources-campania_region.json` is a revised historical catalog of surface releases along the Campania coast. It supports regional screening and comparison with the older `sources-campania_region.json` catalog.

## Prerequisites and input

Provide forcing whose physical domain covers every listed longitude/latitude. Features use zero-metre depth, 100 particles per hour, and sentinel start/end values. Unlike the older catalog, grid indices are not embedded, so the selected adapter must map each physical coordinate to a valid wet cell.

## Configuration and exact use

Set `sources.active:true` and `sources.sources_file:"examples/new-sources-campania_region.json"` in a complete run configuration, then execute:

```bash
./build/wacommplusplus path/to/configuration.json
```

## Expected behavior and validation

Review the log for every accepted/rejected source, plot the initial coordinates over mask and bathymetry, check per-source identities and counts, and compare the mapped cells with the older catalog before treating the revision as scientifically equivalent.

## Limitations, interpretation, and reproducibility

The catalog does not define contaminant mass or observational uncertainty. Any difference from the older file may change emitted trajectories and must be recorded. Archive both catalog checksums, resolved configuration, forcing/restart checksums, revision, seed, toolchain, backend settings, tolerances, tests, and outputs. See [configuration](../docs/configuration.md) and [reproducibility](../docs/reproducibility.md).


## References

- Dagestad, K.-F., Röhrs, J., Breivik, Ø., and Ådlandsvik, B. (2018). OpenDrift v1.0: a generic framework for trajectory modelling. *Geoscientific Model Development*, 11, 1405–1420. [doi:10.5194/gmd-11-1405-2018](https://doi.org/10.5194/gmd-11-1405-2018).
- Montella, R., et al. (2023). A highly scalable high-performance Lagrangian transport and diffusion model for marine pollutants assessment. *Proceedings of PDP 2023*, 17–26. [doi:10.1109/PDP59025.2023.00012](https://doi.org/10.1109/PDP59025.2023.00012).
