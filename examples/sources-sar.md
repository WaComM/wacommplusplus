# Search-and-rescue source

## Scientific objective

`sources-sar.json` initializes a surface search-and-rescue ensemble at 14.043300 E, 40.810010 N at 2021-07-01 09:00 UTC. It is intended for sensitivity and drift demonstrations, not an operational rescue decision.

## Prerequisites and input

Use chronological forcing that contains the point and the emission time. The source has one GeoJSON point, a zero-metre depth, and 10,000 particles per hour. Confirm that the adapter's longitude, latitude, mask, bathymetry, and time coordinates cover those values.

## Configuration and exact use

Set the run configuration's `sources.active` to `true` and `sources.sources_file` to `examples/sources-sar.json`, then run:

```bash
./build/wacommplusplus path/to/configuration.json
```

For a repository scenario, copy `wacomm-native-sar.json`, correct its forcing paths, and run `./build/wacommplusplus examples/wacomm-native-sar.json`.

## Expected behavior and validation

Particles appear only at the declared timestamp and then follow the configured physics. Check the log for source acceptance, verify the emitted identity/count, plot the initial position over the forcing grid, and compare a `random:false` run with a fixed-seed stochastic run.

## Limitations, interpretation, and reproducibility

The point and ensemble do not encode observational uncertainty, windage, object leeway, or rescue probability. Interpret output as candidate drift under the selected circulation and closures. Archive the source/configuration and forcing checksums, revision, seed, toolchain, backend settings, tolerances, tests, and output checksums. Related guidance is in [model](../docs/model.md), [configuration](../docs/configuration.md), and [reproducibility](../docs/reproducibility.md).
