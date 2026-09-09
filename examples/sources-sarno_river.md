# Sarno River source

## Scientific objective

`sources-sarno_river.json` represents continuous surface release from a point near the Sarno River mouth at 14.466590881347654 E, 40.72813686316017 N. It supports coastal-plume sensitivity experiments.

## Prerequisites and input

Provide forcing whose wet grid contains the point. The GeoJSON source requests 10,000 particles per hour and omits explicit start/end times, so the run interval supplies its active window. Verify this behavior against the resolved configuration before production use.

## Configuration and exact use

Set `sources.active:true` and `sources.sources_file:"examples/sources-sarno_river.json"` in a complete run configuration. Then execute:

```bash
./build/wacommplusplus path/to/configuration.json
```

`wacomm-sarno-lite.json` is the related historical scenario; update its `sources_file` and forcing paths before running it.

## Expected behavior and validation

Particles are emitted at the configured rate while the source is active. Confirm the point maps to a wet cell, preserve the expected count, inspect coastline-closure events, and compare serial and requested parallel backends within the declared tolerance.

## Limitations, interpretation, and reproducibility

The source rate is a particle sampling rate, not a calibrated contaminant mass flux. Results describe transport conditional on forcing, resolution, diffusion, and closure assumptions. Archive revision, resolved configuration, source/forcing checksums, seed, dependencies, platform, parallel settings, tolerances, tests, and output checksums. See [model](../docs/model.md) and [testing](../docs/testing.md).


## References

- Dagestad, K.-F., Röhrs, J., Breivik, Ø., and Ådlandsvik, B. (2018). OpenDrift v1.0: a generic framework for trajectory modelling. *Geoscientific Model Development*, 11, 1405–1420. [doi:10.5194/gmd-11-1405-2018](https://doi.org/10.5194/gmd-11-1405-2018).
- Montella, R., et al. (2023). A highly scalable high-performance Lagrangian transport and diffusion model for marine pollutants assessment. *Proceedings of PDP 2023*, 17–26. [doi:10.1109/PDP59025.2023.00012](https://doi.org/10.1109/PDP59025.2023.00012).
