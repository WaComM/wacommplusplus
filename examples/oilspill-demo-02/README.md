# Gaiola oil-spill polygon

## Scientific objective

`oilspill-demo-02.json` initializes 10,000 surface particles around Gaiola for a hypothetical near-shore oil-spill transport demonstration.

## Prerequisites and input

This file is a GeoJSON `Polygon` release area with a particle count and a zero-metre `depth`. It is not a WaComM++ source file. The C++ source reader accepts only `Point` positions or explicit grid indices, and rejects this file with an error naming the feature ([source position](../../docs/configuration.md#source-position)). Earlier revisions accepted it silently and created a source at the sentinel index $10^{37}$, whose particles never entered the domain. The release has no timestamp of its own; the enclosing configuration must define the timing.

## Configuration and exact use

The polygon is intended for the legacy `utilities/createRestartFileFromJson` tool. That tool samples particles uniformly inside the polygon and writes an unversioned NetCDF restart:

```bash
python3 utilities/createRestartFileFromJson/main.py d03.nc examples/oilspill-demo-02/oilspill-demo-02.json 20220915Z0000
```

The resulting `restart_20220915Z0000.nc` is then named in `restart.restart_file`, with `restart.active:true` and `sources.active:false`, and the run is:

```bash
./build/wacommplusplus path/to/configuration.json
```

**This path has not been verified against the current model and should not be used for scientific results.** The tool expects one-dimensional `latitude`, `longitude`, and `depth` axes in the domain file. It stores the index of the nearest node and nearest depth value, as non-negative integers, whereas the solver uses fractional indices with $k\le0$ at the surface ([particle state and coordinates](../../docs/model.md#particle-state-and-coordinates)). It also writes identities as 32-bit floats. Until the tool is brought into line with the model's position mapping and restart format, these demonstrations have no verified execution path.

## Expected behavior and validation

Once the preparation path is verified, the initial particles should lie inside the polygon on valid wet interpolation cells. Plot the initialized cloud with the coastline, verify the total count, and check that no particle lies outside a supported interpolation cell. The tool samples with Python's unseeded `random` module, so each preparation produces a different cloud. Archive the generated restart and its checksum; do not try to reproduce it from `physics.random_seed`, which the tool does not use.

## Limitations, interpretation, and reproducibility

Near-shore behavior is sensitive to grid resolution and closure settings. The scenario contains no oil weathering, mass, windage, or observations and cannot support impact claims by itself. Archive source/configuration and forcing checksums, revision, seed, compiler, dependencies, backend layout, tolerances, tests, and outputs. See [adapters](../../docs/adapters.md) and [reproducibility](../../docs/reproducibility.md).


## Performance evaluation

Apply the [shared strong-scaling and GPU evaluation protocol](../../docs/performance-evaluation.md) to this configuration. Archive a Codex performance-review note for each measured resource tuple; scientific equivalence and workload identity are prerequisites for interpreting timing.

## References

- Dagestad, K.-F., Röhrs, J., Breivik, Ø., and Ådlandsvik, B. (2018). OpenDrift v1.0: a generic framework for trajectory modelling. *Geoscientific Model Development*, 11, 1405–1420. [doi:10.5194/gmd-11-1405-2018](https://doi.org/10.5194/gmd-11-1405-2018).
- Montella, R., et al. (2023). A highly scalable high-performance Lagrangian transport and diffusion model for marine pollutants assessment. *Proceedings of PDP 2023*, 17–26. [doi:10.1109/PDP59025.2023.00012](https://doi.org/10.1109/PDP59025.2023.00012).
