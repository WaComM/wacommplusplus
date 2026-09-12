# Campania regional point sources

## Scientific objective

`sources-campania_region.json` defines a historical set of surface point releases along the Campania coast. It is intended for regional transport screening and regression of source-to-grid mapping.

## Prerequisites and input

Use the forcing grid for which the stored longitude/latitude and legacy `i/j/k` hints were prepared. Each feature requests 100 particles per hour and uses sentinel start/end values. Longitude and latitude are authoritative physical coordinates; verify all points against the current adapter rather than assuming that stored grid indices apply to another product.

## Configuration and exact use

Reference this artifact from a complete configuration:

```json
"sources": {"active": true, "sources_file": "examples/sources-campania_region/sources-campania_region.json"}
```

Then run `./build/wacommplusplus path/to/configuration.json`.

## Expected behavior and validation

Every accepted feature should map to a supported wet interpolation cell and emit at the declared rate. Review rejected-source messages, plot all initial points, preserve expected per-source identities and counts, and compare deterministic serial and parallel results.

## Limitations, interpretation, and reproducibility

The points and sampling rates are not calibrated pollutant loads. Stored grid hints are product-specific and may be stale. Interpret trajectories as transport conditional on forcing and model settings. Archive revision, resolved configuration, source/forcing checksums, seed, dependencies, platform/backends, tolerances, tests, and output checksums. See [adapters](../../docs/adapters.md) and [testing](../../docs/testing.md).


## References

- Dagestad, K.-F., Röhrs, J., Breivik, Ø., and Ådlandsvik, B. (2018). OpenDrift v1.0: a generic framework for trajectory modelling. *Geoscientific Model Development*, 11, 1405–1420. [doi:10.5194/gmd-11-1405-2018](https://doi.org/10.5194/gmd-11-1405-2018).
- Montella, R., et al. (2023). A highly scalable high-performance Lagrangian transport and diffusion model for marine pollutants assessment. *Proceedings of PDP 2023*, 17–26. [doi:10.1109/PDP59025.2023.00012](https://doi.org/10.1109/PDP59025.2023.00012).
