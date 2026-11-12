# WaComM++ utility for creating a restart file from GeoJSON polygons

`createRestartFileFromJson` samples particles inside GeoJSON `Polygon` release areas and writes them as the initial NetCDF restart of a WaComM++ run. It is part of the C++ application build, uses the solver's ocean adapters and position mapping, and writes the restart with the production writer (`wacomm_restart_version` 3). The sampling method is defined in [polygon releases](../../docs/model.md#polygon-releases) and the file format in [polygon release files](../../docs/configuration.md#polygon-release-files).

## Build

The tool is built and installed with `wacommplusplus` when `BUILD_APPLICATION=ON` (the default):

```bash
cmake -S . -B build
cmake --build build --target createRestartFileFromJson wacommplusplus
```

## Usage

```bash
./build/createRestartFileFromJson <configuration.json> <polygons.geojson> <YYYYMMDDZhhmm>
```

### input

_configuration.json_: the WaComM++ configuration that will run from the restart. It must set `restart.active:true` and a `restart.restart_file` ending in `.nc`. The grid is read from `io.ocean_model` and from the first window of `io.nc_inputs` in `tracking.direction`. The positions are keyed by `physics.random_seed`, and the drift object and side come from the `drift` settings. \
_polygons.geojson_: a `FeatureCollection` of `Polygon` features with `[longitude, latitude]` positions in degrees, in the grid's longitude convention, and the properties `particles` (a positive integer) and `depth` (metres below the surface, non-negative). Holes are allowed. Invalid geometry fails with the feature name. \
_YYYYMMDDZhhmm_: the UTC release time, in the calendar format of `simulation.start` (also `YYYYMMDDZhh` or `YYYYMMDDhh`). It must lie inside the first forcing window.

### output

The file named by `restart.restart_file`. It holds every sampled particle with unsigned 64-bit identities from 0, fractional indices `k, j, i` computed by `OceanModelAdapter::deplatlon2kji` (`k <= 0`, with the surface at 0), age 0, initial health, and emission time equal to the release time. It records `checkpoint_time`, `random_seed`, `tracking_direction`, `ocean_model`, the build provenance, and the full configuration, together with `wacomm_polygon_release_file`, `wacomm_polygon_release_sampler`, and `wacomm_polygon_release_time`. The tool reads the file back with the production loader and exits non-zero unless every particle is reproduced exactly. It also stops on positions outside the interpolation domain and on samples in an interpolation cell with a land node.

## Wacomm++

In the configuration passed to both programs, set

```json
    "restart": {
      "active": true,
      "restart_file": "restart_20220915Z0000.nc"
    },
    "sources": {
      "active": false
    },
```

then run

```bash
./build/wacommplusplus configuration.json
```

The run starts at the release time in the configured direction.

## Verification and limitations

`polygon_release` and `application_end_to_end` verify the tool on fixture grids, the second through the production executable, forward and backward ([testing](../../docs/testing.md), [application verification](../../docs/application-verification.md)). Samples are uniform per unit spherical area inside polygons whose edges are straight in longitude and latitude. They are unit-weight computational particles, not an oil volume or mass distribution. Polygons spanning 180° of longitude or more are rejected.

This tool replaces the earlier `main.py`. That script placed particles at the nearest node of one-dimensional axes, stored non-negative integer indices and 32-bit float identities, sampled with Python's unseeded `random`, and wrote an unversioned restart. It has been removed.

## Tutorial

The [oil-spill demonstrations](../../examples/oilspill-demo-01/README.md) are polygon release files for this tool:

```bash
./build/createRestartFileFromJson path/to/configuration.json examples/oilspill-demo-01/oilspill-demo-01.json 20220915Z0000
./build/wacommplusplus path/to/configuration.json
```

## Reproducibility

Archive the polygon file, configuration, release-time argument, forcing checksums, Git revision, `physics.random_seed`, compiler and CMake options, and the checksum of the generated restart. The sampled longitudes and latitudes are bitwise identical on every supported platform: the sampler uses only basic IEEE-754 operations, without floating-point contraction or C library trigonometry. The fractional indices then come from `deplatlon2kji`, and can differ in their last bits between compilers that fuse multiply–add and those that do not. With the same build and platform, the restart's particle state is reproduced bit for bit.

## References

- Hormann, K., and Agathos, A. (2001). The point in polygon problem for arbitrary polygons. *Computational Geometry*, 20(3), 131–144. [doi:10.1016/S0925-7721(01)00012-8](https://doi.org/10.1016/S0925-7721(01)00012-8).
- Montella, R., et al. (2023). A highly scalable high-performance Lagrangian transport and diffusion model for marine pollutants assessment. *Proceedings of PDP 2023*, 17–26. [doi:10.1109/PDP59025.2023.00012](https://doi.org/10.1109/PDP59025.2023.00012).

Interoperability (not a scientific source): Butler, H., Daly, M., Doyle, A., Gillies, S., Hagen, S., and Schaub, T. (2016). *The GeoJSON Format*. RFC 7946, IETF. [doi:10.17487/RFC7946](https://doi.org/10.17487/RFC7946).
