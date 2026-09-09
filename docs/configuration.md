# Configuration

Configuration is JSON or the legacy namelist form. New scientific runs should set an explicit seed and direction:

```json
{"physics":{"random":false,"random_seed":5489,"dti":30},"tracking":{"direction":"forward","backward_diffusion":"none"}}
```

`direction` is `forward` or `backward`. Backward diffusion defaults to `none`; `symmetric_stochastic` enables an ensemble and must not be interpreted as exact path reconstruction. Closure values are `constraint`, `kill`, and `reflection`. Unknown direction and backward-diffusion values are errors.

`physics.dti` and `physics.deltat` are seconds and must be finite and greater than zero. `physics.sigma` is a stochastic displacement scale in meters and `physics.shore_limit` is a positive-down water-column threshold in meters; both must be finite and non-negative. Invalid numerical or closure values fail during configuration loading rather than entering the integration loop.

Surface-object drift is opt-in:

```json
{
  "drift": {
    "model": "leeway",
    "object_type": "PERSON_IN_WATER",
    "side": "right"
  },
  "environment": {
    "wind": {
      "adapter": "constant",
      "u10": 5.0,
      "v10": 0.0
    }
  }
}
```

`drift.model` is `passive` or `leeway`. Passive is the backward-compatible default and does not request atmospheric input. Leeway requires one of `PERSON_IN_WATER`, `LIFERAFT_NO_DROGUE`, `LIFERAFT_DROGUE`, `GENERIC_VESSEL`, or `SHIPPING_CONTAINER`; it also requires `side=left|right`. Stable numeric object identifiers are persisted in restart files, while symbolic names remain the public configuration interface.

The first environmental provider is `environment.wind.adapter=constant`. Components `u10` and `v10` are finite eastward and northward 10 m wind velocities in m s-1. This provider is appropriate for analytical verification, controlled sensitivity studies, and spatially uniform forcing intervals. It is not a substitute for resolved atmospheric forcing in operational applications. Unknown models, object types, orientations, wind adapters, missing components, and non-finite winds fail during configuration loading.

Resolved WRF wind and WW3 Stokes drift use parallel forcing lists:

```json
"environment": {
  "wind": {"adapter":"WRF", "nc_inputs":["wrf.nc"], "regrid":"none"},
  "wave": {"adapter":"WW3", "nc_inputs":["ww3.nc"], "regrid":"bilinear_geographic"}
}
```

Each list must contain exactly one file for every entry in `io.nc_inputs`. Files must normalize to the same chronological timestamps and horizontal coordinates as the ocean particle grid. WRF wind is required by leeway; WW3 Stokes drift is optional and is added only for non-passive drift objects. Dynamic WRF/WW3 coupling currently runs on serial CPU, OpenMP, MPI, and FlexMPI/EMPI paths. A CUDA device causes an actionable failure because dynamic environmental arrays have not yet been ported to device memory; constant wind remains CUDA-compatible.

Environmental variables must carry explicit metadata. Velocity uses `m s-1` or an accepted spelling of meters per second; longitude and latitude use east/north angular units. Numeric time uses CF-style `<unit> since <UTC reference>` metadata with seconds, minutes, hours, or days and a supported Gregorian calendar. WaComM++ converts the coordinate to seconds since 1968-05-23 before matching it against ocean time. It rejects unsupported calendars and units rather than guessing.

`regrid` is `none` by default. `bilinear_geographic` is the only initial opt-in operator. It accepts a monotonic rectilinear environmental longitude/latitude grid, requires every ocean-grid point to be inside the source extent, and interpolates already Earth-relative vector components. It rejects projected, curvilinear-source, antimeridian-crossing, and extrapolation cases. The operator is exact for affine coordinate fields but is not conservative; configuration therefore records the scientific choice explicitly.

The complete resolved JSON configuration, including defaults, is embedded in NetCDF outputs. Reproducible experiments should nevertheless archive the original configuration, forcing and restart checksums, build metadata, parallel layout, tolerances, and output checksums.

## References

- Dagestad, K.-F., Röhrs, J., Breivik, Ø., and Ådlandsvik, B. (2018). OpenDrift v1.0: a generic framework for trajectory modelling. *Geoscientific Model Development*, 11, 1405–1420. [doi:10.5194/gmd-11-1405-2018](https://doi.org/10.5194/gmd-11-1405-2018).
- Breivik, Ø., Allen, A. A., Maisondieu, C., and Roth, J.-C. (2011). Wind-induced drift of objects at sea: the leeway field method. *Applied Ocean Research*, 33, 100–109. [doi:10.1016/j.apor.2011.01.005](https://doi.org/10.1016/j.apor.2011.01.005).
- Thygesen, U. H. (2011). How to reverse time in stochastic particle tracking models. *Journal of Marine Systems*, 88, 159–168. [doi:10.1016/j.jmarsys.2011.03.009](https://doi.org/10.1016/j.jmarsys.2011.03.009).
- Hassell, D., Gregory, J., Blower, J., Lawrence, B. N., and Taylor, K. E. (2017). A data model of the Climate and Forecast metadata conventions (CF-1.6) with a software implementation (cf-python v2.1). *Geoscientific Model Development*, 10, 4619–4646. [doi:10.5194/gmd-10-4619-2017](https://doi.org/10.5194/gmd-10-4619-2017).
- Jones, P. W. (1999). First- and second-order conservative remapping schemes for grids in spherical coordinates. *Monthly Weather Review*, 127, 2204–2210. [doi:10.1175/1520-0493(1999)127%3C2204:FASOCR%3E2.0.CO;2](https://doi.org/10.1175/1520-0493%281999%29127%3C2204%3AFASOCR%3E2.0.CO%3B2).
