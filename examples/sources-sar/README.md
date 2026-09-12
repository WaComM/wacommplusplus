# Search-and-rescue source

## Scientific objective

`sources-sar.json` initializes a surface search-and-rescue ensemble at 14.043300 E, 40.810010 N at 2021-07-01 09:00 UTC. It is intended for sensitivity and drift demonstrations, not an operational rescue decision.

This file defines release geometry only and cannot activate leeway. A complete run remains passive unless its runtime configuration explicitly selects the leeway model, object class, crosswind side, and valid 10 m wind provider.

## Prerequisites and input

Use chronological forcing that contains the point and the emission time. The source has one GeoJSON point, a zero-metre depth, and 10,000 particles per hour. Confirm that the adapter's longitude, latitude, mask, bathymetry, and time coordinates cover those values.

## Configuration and exact use

Set the run configuration's `sources.active` to `true` and `sources.sources_file` to `examples/sources-sar/sources-sar.json`, then run:

```bash
./build/wacommplusplus path/to/configuration.json
```

For a repository scenario, copy `wacomm-native-sar.json`, correct its forcing paths, and run `./build/wacommplusplus examples/wacomm-native-sar/wacomm-native-sar.json`.

## Expected behavior and validation

Particles appear only at the declared timestamp and then follow the configured physics. Check the log for source acceptance, verify the emitted identity/count, plot the initial position over the forcing grid, and compare a `random:false` run with a fixed-seed stochastic run.

## Limitations, interpretation, and reproducibility

The point and ensemble do not encode observational uncertainty, windage, object leeway, or rescue probability. Interpret output as candidate drift under the selected circulation and closures. Archive the source/configuration and forcing checksums, revision, seed, toolchain, backend settings, tolerances, tests, and output checksums. Related guidance is in [model](../../docs/model.md), [configuration](../../docs/configuration.md), and [reproducibility](../../docs/reproducibility.md).


## References

- Dagestad, K.-F., Röhrs, J., Breivik, Ø., and Ådlandsvik, B. (2018). OpenDrift v1.0: a generic framework for trajectory modelling. *Geoscientific Model Development*, 11, 1405–1420. [doi:10.5194/gmd-11-1405-2018](https://doi.org/10.5194/gmd-11-1405-2018).
- Montella, R., et al. (2023). A highly scalable high-performance Lagrangian transport and diffusion model for marine pollutants assessment. *Proceedings of PDP 2023*, 17–26. [doi:10.1109/PDP59025.2023.00012](https://doi.org/10.1109/PDP59025.2023.00012).
