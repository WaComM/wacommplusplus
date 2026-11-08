# Capri–Punta Campanella oil-spill polygon

## Scientific objective

`oilspill-demo-01.json` initializes 10,000 surface particles inside a polygon between Capri and Punta Campanella for a hypothetical oil-spill transport demonstration. The scientific question is where unit-weight passive particles, released at the same instant uniformly over this open-water area, are carried by the configured currents. It is a passive-transport scenario: no drift model is selected by the polygon, and the particles follow whatever `drift` settings the run configuration declares (passive by default).

## Prerequisites and input

This file is a GeoJSON `FeatureCollection` with one `Polygon` feature: a 24-vertex exterior ring, about 0.009° × 0.006° (roughly 0.8 km × 0.7 km), with no holes. Its properties are `particles: 10000` and `depth: 0` (metres below the surface). It is not a WaComM++ source file, and the source reader rejects it ([source position](../../docs/configuration.md#source-position)). Instead it is a [polygon release file](../../docs/configuration.md#polygon-release-files). The release has no timestamp of its own; the time is given on the command line.

You also need:

- an application build (`BUILD_APPLICATION=ON`), which provides `wacommplusplus` and `createRestartFileFromJson`;
- ocean forcing that covers the Gulf of Naples and the release time, in a supported adapter format (for example ROMS output with `ocean_time` [s], `u`, `v` [m s⁻¹], `w` [m s⁻¹], `AKt` [m² s⁻¹], `zeta` [m], `h` [m], `mask_rho`, `lon_rho`, `lat_rho` [degrees], and `s_w`; see [adapters](../../docs/adapters.md));
- a complete run configuration with `io.ocean_model` and `io.nc_inputs` for that forcing, `restart.active:true`, `restart.restart_file` ending in `.nc`, an explicit `physics.random_seed` and `physics.integrator`, and `tracking.direction`. Set `sources.active:false` unless other sources are intended.

## Configuration and exact use

Stage the forcing under [`data/`](data/README.md) or another explicit path, then generate the initial restart and run:

```bash
./build/createRestartFileFromJson path/to/configuration.json examples/oilspill-demo-01/oilspill-demo-01.json 20220915Z0000
./build/wacommplusplus path/to/configuration.json
```

The first command reads the grid from the first forcing window in the tracking direction. It samples the polygon uniformly per unit spherical area, using `physics.random_seed` keyed by particle identity. It maps each position with the same exact inverse as Point sources, and writes a version 3 restart to `restart.restart_file`. The release time `20220915Z0000` must lie inside that first window. The run then starts at the release time, forward or backward according to the configuration. The method is defined in [polygon releases](../../docs/model.md#polygon-releases).

## Expected behavior and validation

The tool prints the particle count, release time, and seed. The restart holds 10,000 particles with identities 0–9,999, health $H_0$, age 0, emission time equal to the release time, and $k=0$ (surface). The tool reads the restart back with the production loader and exits non-zero unless every particle is reproduced exactly. It also fails if a sample lies outside the interpolation domain or in a cell with a land node. The polygon lies in open water in the strait, so on a sufficiently fine grid every sample should fall in a wet cell.

Check the generated restart before running: the particle count, that every `lon`/`lat` lies inside the polygon, and that the file attributes record `random_seed`, `tracking_direction`, `checkpoint_time`, and `wacomm_polygon_release_sampler`. Generating twice with the same configuration and seed must give identical `i`, `j`, and `k`. A different seed gives a different cloud of the same distribution.

What has been verified: `polygon_release` checks this sampler on a fixture grid (containment, count, identities, seeded reproducibility, area-uniform density, failure cases, exact restart round trip), and also checks that this polygon passes validation. `application_end_to_end` runs the same tool and the production executable forward and backward on an analytic current, and requires every particle to match the analytic solution within 0.1 % of its path ([application verification](../../docs/application-verification.md)). Every sampled interpolation cell inside this polygon has four wet nodes on the 1688 × 2273 Campania ROMS grid `Campania_max135m_withC3andC4_angle0_hmin_2_5.nc` (about 0.0012° × 0.0009°; not distributed with the repository), so the land rule does not stop the release on that grid. A coarser grid must be checked again. This demonstration has not been run here on real Gulf of Naples forcing, so no expected trajectories, figures, or checksums are published for it.

## Limitations, interpretation, and reproducibility

This hypothetical release does not specify oil chemistry, weathering, windage, mass, or observations. Interpret it only as particle transport under the configured model. The 10,000 particles are a finite, area-uniform computational sample, not a distribution of oil volume or thickness. All particles share one depth and one release instant. A backward run from this polygon is a candidate-origin ensemble for the area, not an inverse of a particular forward cloud. Archive the polygon file, the run configuration, the release-time argument, forcing checksums, the Git revision, `physics.random_seed`, compiler and CMake options, backend settings, tolerances, test results, and the checksums of the generated restart and outputs. See [configuration](../../docs/configuration.md), [model](../../docs/model.md), and [reproducibility](../../docs/reproducibility.md).


## Performance evaluation

Apply the [shared strong-scaling and GPU evaluation protocol](../../docs/performance-evaluation.md) to this configuration. Archive a Codex performance-review note for each measured resource tuple; scientific equivalence and workload identity are prerequisites for interpreting timing.

## References

- Dagestad, K.-F., Röhrs, J., Breivik, Ø., and Ådlandsvik, B. (2018). OpenDrift v1.0: a generic framework for trajectory modelling. *Geoscientific Model Development*, 11, 1405–1420. [doi:10.5194/gmd-11-1405-2018](https://doi.org/10.5194/gmd-11-1405-2018).
- Hormann, K., and Agathos, A. (2001). The point in polygon problem for arbitrary polygons. *Computational Geometry*, 20(3), 131–144. [doi:10.1016/S0925-7721(01)00012-8](https://doi.org/10.1016/S0925-7721(01)00012-8).
- Montella, R., et al. (2023). A highly scalable high-performance Lagrangian transport and diffusion model for marine pollutants assessment. *Proceedings of PDP 2023*, 17–26. [doi:10.1109/PDP59025.2023.00012](https://doi.org/10.1109/PDP59025.2023.00012).
