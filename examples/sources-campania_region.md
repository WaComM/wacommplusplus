# Campania regional point sources

## Scientific objective

`sources-campania_region.json` defines a historical set of surface point releases along the Campania coast. It is intended for regional transport screening and regression of source-to-grid mapping.

## Prerequisites and input

Use the forcing grid for which the stored longitude/latitude and legacy `i/j/k` hints were prepared. Each feature requests 100 particles per hour and uses sentinel start/end values. Longitude and latitude are authoritative physical coordinates; verify all points against the current adapter rather than assuming that stored grid indices apply to another product.

## Configuration and exact use

Reference this artifact from a complete configuration:

```json
"sources": {"active": true, "sources_file": "examples/sources-campania_region.json"}
```

Then run `./build/wacommplusplus path/to/configuration.json`.

## Expected behavior and validation

Every accepted feature should map to a supported wet interpolation cell and emit at the declared rate. Review rejected-source messages, plot all initial points, preserve expected per-source identities and counts, and compare deterministic serial and parallel results.

## Limitations, interpretation, and reproducibility

The points and sampling rates are not calibrated pollutant loads. Stored grid hints are product-specific and may be stale. Interpret trajectories as transport conditional on forcing and model settings. Archive revision, resolved configuration, source/forcing checksums, seed, dependencies, platform/backends, tolerances, tests, and output checksums. See [adapters](../docs/adapters.md) and [testing](../docs/testing.md).
