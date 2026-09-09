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

The complete resolved JSON configuration, including defaults, is embedded in NetCDF outputs. Reproducible experiments should nevertheless archive the original configuration, forcing and restart checksums, build metadata, parallel layout, tolerances, and output checksums.

## References

- Dagestad, K.-F., Röhrs, J., Breivik, Ø., and Ådlandsvik, B. (2018). OpenDrift v1.0: a generic framework for trajectory modelling. *Geoscientific Model Development*, 11, 1405–1420. [doi:10.5194/gmd-11-1405-2018](https://doi.org/10.5194/gmd-11-1405-2018).
- Breivik, Ø., Allen, A. A., Maisondieu, C., and Roth, J.-C. (2011). Wind-induced drift of objects at sea: the leeway field method. *Applied Ocean Research*, 33, 100–109. [doi:10.1016/j.apor.2011.01.005](https://doi.org/10.1016/j.apor.2011.01.005).
- Thygesen, U. H. (2011). How to reverse time in stochastic particle tracking models. *Journal of Marine Systems*, 88, 159–168. [doi:10.1016/j.jmarsys.2011.03.009](https://doi.org/10.1016/j.jmarsys.2011.03.009).
