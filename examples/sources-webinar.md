# Webinar point source

## Scientific objective

`sources-webinar.json` is the high-density surface point source used by the historical WaComM++ webinar scenario near the Campania coast. It is retained to reproduce that demonstration.

## Prerequisites and input

The source requires a forcing grid containing 14.0468 E, 40.83480299863682 N. Its `start` and `end` sentinel values defer activity to the simulation interval, and it requests 250,000 particles per hour; confirm memory and runtime capacity before execution.

## Configuration and exact use

Reference the file from a complete configuration with `sources.active:true`. The matching historical configuration uses a generic `sources.json`, so copy this artifact to that name or update the path explicitly:

```bash
./build/wacommplusplus examples/webinar-native-usecase.json
```

## Expected behavior and validation

The run should accept the point as wet and emit at the requested rate. Check logs, particle totals, initial grid placement, and output timestamps. Run a smaller particle count first and compare deterministic serial/OpenMP results before a large parallel run.

## Limitations, interpretation, and reproducibility

This is a demonstration sampling density, not a measured discharge. The external 2019 forcing is not distributed here. Record all configuration/source changes, input checksums, revision, seed, compiler, dependencies, backend layout, tolerances, tests, and output checksums. See [parallelism](../docs/parallelism.md) and [reproducibility](../docs/reproducibility.md).
