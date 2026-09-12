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
    "side": "right",
    "coefficient_ensemble": false
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

`drift.model` is `passive` or `leeway`. Passive is the backward-compatible default and does not request atmospheric input. Leeway requires an `object_type` symbolic name from the [object catalog](catalogs.md) and `side=left|right|random`. Stable numeric identifiers 0--5 retain their original meanings, new catalog entries use subsequent identifiers, and resolved sides are persisted in restart files. Names, rather than numeric identifiers, are the public configuration interface. Some catalog entries have distinct left and right regressions; selecting a side therefore chooses the corresponding measured coefficient triplet rather than merely negating a common magnitude.

`drift.coefficient_ensemble` defaults to `false`. When true, the catalog standard deviations scale Gaussian residual velocities in the downwind and crosswind regressions. `drift.residual_correlation=rho` defaults to zero, requires the ensemble, and must lie in [-1,1]. If `z1,z2` are independent standard normals, the implementation uses `z_DW=z1` and `z_CW=rho z1+sqrt(1-rho^2) z2`, giving correlation `rho` while retaining unit marginal variance. The pair is a deterministic function of `physics.random_seed` and stable particle identity and remains fixed for that trajectory. A nonzero value is a user-supplied covariance hypothesis, not a cataloged empirical result.

`drift.side=random` requires an explicit `side_right_probability` in the closed interval [0,1]. For stable identity $n$, a counter-key uniform variate $U_n$ assigns right when $U_n<p_R$ and left otherwise. This samples an initial categorical modeling prior once; it is not a time-dependent jibing process. Fixed `left` or `right` configurations reject `side_right_probability` so an unused uncertainty parameter cannot be silently archived. The resolved side is stored with each particle and is invariant under direction, restart, rank, thread, and accelerator scheduling.

`drift.jibe_probability_per_hour` defaults to 0 and accepts [0,1] only for leeway runs. It defines a constant exponential waiting-time hazard, not a universal catalog value. WaComM++ converts it to the probability of a resolved transition over each physical integration step. A successful keyed draw flips the side after that step; at most one transition is resolved per substep. The probability, seed, `dti`, and resolved particle state are therefore required reproducibility metadata.

The first environmental provider is `environment.wind.adapter=constant`. Components `u10` and `v10` are finite eastward and northward 10 m wind velocities in m s-1. This provider is appropriate for analytical verification, controlled sensitivity studies, and spatially uniform forcing intervals. It is not a substitute for resolved atmospheric forcing in operational applications. Unknown models, object types, orientations, wind adapters, missing components, and non-finite winds fail during configuration loading.

`environment.wind.uncertainty_stddev=sigma_W` defaults to zero, is measured in m s-1, and is valid only for leeway runs. `uncertainty_component_correlation=rho_W` defaults to zero and must be finite in [-1,1]; with equal component variance this validates the positive-semidefinite covariance `sigma_W^2 [[1,rho_W],[rho_W,1]]`. `uncertainty_spatial_scale=L_W` in metres and `uncertainty_temporal_scale=T_W` in seconds are finite and nonnegative. Positive scales select a shared piecewise-constant Gaussian field in Earth-relative equirectangular spatial bins and absolute physical-time bins. Samples have perfect covariance inside the same bin and zero covariance across a bin boundary; these are numerical support widths, not e-folding correlation lengths. Longitude is multiplied by cosine(latitude), so this inexpensive operator is unsuitable near the poles or across the antimeridian. Zero spatial or temporal scale retains particle-keyed or integration-substep white noise respectively, and all-zero correlation settings preserve the legacy realization exactly. Nonzero correlation settings require `uncertainty_stddev>0`. These values are sensitivity hypotheses unless their estimator and forcing-specific observational provenance are archived.

`observational_calibration.regions` is an opt-in array of at most 16 non-overlapping, closed EPSG:4326 rectangles. At each substep the particle midpoint selects a region; its `wind_error` values replace the global wind-error standard deviation, component correlation, spatial scale, and temporal scale. Outside all regions the global values apply. Boundary sharing is rejected because closed domains would otherwise be ambiguous; antimeridian-spanning rectangles are unsupported. Selection changes parameters, not the stochastic key, equations, interpolation, vector basis, or direction policy.

Each region requires `id`, `crs="EPSG:4326"`, `bounds` (`west`, `south`, `east`, `north` in degrees), the exact configured `object_type` and `forcing_adapter`, ISO-8601 `valid_from` and `valid_until` provenance strings, a named `estimator`, `sample_size>=2`, dataset identity, `dataset_checksum="sha256:<64 hex digits>"`, a DOI, and `wind_error` containing positive `stddev` (m s-1), PSD `component_correlation`, and nonnegative `spatial_scale` (m) and `temporal_scale` (s). The loader verifies structural consistency, ranges, covariance PSD, and spatial ambiguity; it cannot verify the scientific truth of a DOI, estimator, dataset, period, or geographic applicability. Those remain the investigator's responsibility. A declaration is not a posterior, confidence region, or universal calibration.

```json
"observational_calibration": {"regions": [{
  "id": "study-region-object-forcing-v1", "crs": "EPSG:4326",
  "bounds": {"west": 13.8, "south": 40.5, "east": 14.5, "north": 41.1},
  "object_type": "PERSON_IN_WATER", "forcing_adapter": "WRF",
  "valid_from": "2024-01-01T00:00:00Z", "valid_until": "2024-12-31T23:59:59Z",
  "estimator": "document the residual selection and covariance estimator", "sample_size": 42,
  "dataset": "persistent archived dataset identifier",
  "dataset_checksum": "sha256:0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef",
  "doi": "10.xxxx/replace-with-the-study-doi",
  "wind_error": {"stddev": 1.2, "component_correlation": 0.25,
                 "spatial_scale": 8000.0, "temporal_scale": 1800.0}
}]}
```

The block is a schema illustration with placeholder provenance and values, not a calibrated configuration suitable for a scientific run.

Resolved WRF wind and WW3 Stokes drift use parallel forcing lists:

```json
"environment": {
  "wind": {"adapter":"WRF", "nc_inputs":["wrf.nc"], "regrid":"bilinear_curvilinear_geographic"},
  "wave": {"adapter":"WW3", "nc_inputs":["ww3.nc"], "regrid":"bilinear_geographic"}
}
```

Every forcing entry may be a local path or an explicit `http`, `https`, or `dap4` NetCDF URI. Relative ocean entries are resolved against `io.base_path`; relative WRF and WW3 entries use the same base by default and may declare their own `base_path`. An absolute path or complete URI is never prefixed. Unsupported schemes, missing URI hosts, and embedded URI credentials are rejected; authentication must be configured through the NetCDF transport outside the archived configuration so secrets are not written into provenance.

Access is lazy by forcing window: the application opens and normalizes the current window and the single adjacent boundary record needed for continuous temporal interpolation. The adjacent normalized dataset is retained for the following window, then released, so a chronological run does not eagerly open the full remote collection and does not download the same window twice. NetCDF controls HTTP/DAP transfer and cache behavior. A transport error is fatal and never selects a different product, cached scientific value, or fallback field.

Each list must contain exactly one file for every entry in `io.nc_inputs`. Files must normalize to the same chronological timestamps and horizontal coordinates as the ocean particle grid. WRF wind is required by leeway; WW3 Stokes drift is optional and is added only for non-passive drift objects. Dynamic WRF/WW3 coupling runs on serial CPU, OpenMP, MPI, FlexMPI/EMPI, and CUDA paths. CUDA receives the already validated, rotated, and regridded common-grid arrays; it does not parse product metadata or apply an independent adapter policy.

Environmental variables must carry explicit metadata. Velocity uses `m s-1` or an accepted spelling of meters per second; longitude and latitude use east/north angular units. Numeric time uses CF-style `<unit> since <UTC reference>` metadata with seconds, minutes, hours, or days and a supported Gregorian calendar. WaComM++ converts the coordinate to seconds since 1968-05-23 before matching it against ocean time. It rejects unsupported calendars and units rather than guessing.

`regrid` is `none` by default. `bilinear_geographic` accepts a monotonic rectilinear environmental longitude/latitude grid, including increasing or decreasing axes and longitude axes crossing the antimeridian. `bilinear_curvilinear_geographic` uses inverse bilinear coordinates in a valid, non-folded curvilinear quadrilateral. Both require every ocean-grid point to lie inside the source domain and interpolate already Earth-relative vector components. They reject projected coordinates and extrapolation. Neither operator is conservative; configuration therefore records the scientific choice explicitly.

`bilinear_projected` requires an explicit `source_crs`, for example `"EPSG:3857"`, and `USE_PROJ=ON`. The file must expose rectilinear axes or a valid curvilinear `x/y` mesh in meters. PROJ transforms ocean targets from EPSG:4326 into the declared source CRS before rectilinear or inverse-bilinear interpolation. A CRS is never inferred. Missing or misplaced CRS declarations, unavailable PROJ support, invalid transforms, non-metric coordinates, folded geometry, and extrapolation are errors.

The library-level `conservativeRectilinearGeographicCellAverage` and `conservativeCurvilinearGeographicCellAverage` operators are not valid `environment.wind.regrid` or `environment.wave.regrid` values. The curvilinear API accepts explicit source/target corner bounds, source cell-average scalar data, active fractions in [0,1], and an optional limited second-order mode. This restriction prevents a conservation label from being attached to point-valued velocity components without a scientifically defined flux and vector-basis treatment.

The complete resolved JSON configuration, including defaults, is embedded in NetCDF outputs. Reproducible experiments should nevertheless archive the original configuration, forcing and restart checksums, build metadata, parallel layout, tolerances, and output checksums.

The private Unix application dependency stack (other than macOS) includes OpenSSL 3.5.8 for HTTPS transport. `WACOMM_BOOTSTRAP_DEPENDENCIES` controls dependency selection at CMake configuration time; no simulation JSON key selects TLS. See the [build guide](build.md#serial-build-with-private-openssl) for prerequisites, serial commands, and provenance checks.

The six-hour [Sarno example](../examples/wacomm-sarno-lite.md) sets `simulation.dry=false`, `simulation.end="20210701Z15"`, `io.ocean_model="ROMS"`, `io.save_input=true`, and `io.save_history="nc"`. `history_root` stores physical-time particle snapshots separately from `nc_output_root` gridded counts. The explicit available input list controls the intervals; the missing 12:00 record produces a two-hour interpolation interval and no source batch at 12:00. Example runtime roots are `data/<example_name>/`. Saved native filenames use hour-only stamps.

The MPI Sarno workflow uses the identical scientific configuration and a separate `data/wacomm-sarno-lite/mpi2/` run root; process count is selected by the Slurm wrapper, not a tracking or adapter option.

The MPI/OpenMP scaling sweep derives a native-forcing configuration from the Sarno example, with `ocean_model="WaComM"`, `base_path="processed-6h/"`, the six `ocm3_d03_20210701ZHH.nc` inputs, and `save_input=false`. It uses the same `dry=false` six-hour scientific settings at every process count. Only Slurm and OpenMP execution settings vary; runtime roots are `data/wacomm-sarno-lite/scaling/pN/`. Particle and gridded writes remain enabled; normalized forcing is read-only and is not rewritten.

Saved native windows can contain an adjacent boundary already used for interpolation. Equal-time endpoint records are reused only when all five stored dynamic fields agree exactly; conflicting overlaps fail. `save_input=false` prevents rewriting the shared forcing in the scaling workflow. See [native boundary replay](adapters.md#reusing-saved-native-boundary-records).

The native adapter accepts both `WACOMM` and the historical documented spelling `WaComM`; both select the same implementation and field conventions.

The OpenMP scaling sweep changes only Slurm CPUs per task and OpenMP thread settings, keeping one MPI process and the identical native-forcing scientific configuration from the MPI sweep. Its roots are `data/wacomm-sarno-lite/openmp-scaling/tN/`. No adapter, seed, tracking, restart, source, or output option is changed.

## References

- Dagestad, K.-F., Röhrs, J., Breivik, Ø., and Ådlandsvik, B. (2018). OpenDrift v1.0: a generic framework for trajectory modelling. *Geoscientific Model Development*, 11, 1405–1420. [doi:10.5194/gmd-11-1405-2018](https://doi.org/10.5194/gmd-11-1405-2018).
- Breivik, Ø., Allen, A. A., Maisondieu, C., and Roth, J.-C. (2011). Wind-induced drift of objects at sea: the leeway field method. *Applied Ocean Research*, 33, 100–109. [doi:10.1016/j.apor.2011.01.005](https://doi.org/10.1016/j.apor.2011.01.005).
- Breivik, Ø., and Allen, A. A. (2008). An operational search and rescue model for the Norwegian Sea and the North Sea. *Journal of Marine Systems*, 69, 99–113. [doi:10.1016/j.jmarsys.2007.02.010](https://doi.org/10.1016/j.jmarsys.2007.02.010).
- Coppini, G., Jansen, E., Turrisi, G., Creti, S., Shchekinova, E. Y., Pinardi, N., Lecci, R., Carluccio, I., Kumkar, Y. V., D'Anca, A., Mannarini, G., Martinelli, S., Marra, P., Capodiferro, T., and Gismondi, T. (2016). A new search-and-rescue service in the Mediterranean Sea: a demonstration of the operational capability and an evaluation of its performance using real case scenarios. *Natural Hazards and Earth System Sciences*, 16, 2713–2727. [doi:10.5194/nhess-16-2713-2016](https://doi.org/10.5194/nhess-16-2713-2016).
- Thygesen, U. H. (2011). How to reverse time in stochastic particle tracking models. *Journal of Marine Systems*, 88, 159–168. [doi:10.1016/j.jmarsys.2011.03.009](https://doi.org/10.1016/j.jmarsys.2011.03.009).
- Hassell, D., Gregory, J., Blower, J., Lawrence, B. N., and Taylor, K. E. (2017). A data model of the Climate and Forecast metadata conventions (CF-1.6) with a software implementation (cf-python v2.1). *Geoscientific Model Development*, 10, 4619–4646. [doi:10.5194/gmd-10-4619-2017](https://doi.org/10.5194/gmd-10-4619-2017).
- Jones, P. W. (1999). First- and second-order conservative remapping schemes for grids in spherical coordinates. *Monthly Weather Review*, 127, 2204–2210. [doi:10.1175/1520-0493(1999)127%3C2204:FASOCR%3E2.0.CO;2](https://doi.org/10.1175/1520-0493%281999%29127%3C2204%3AFASOCR%3E2.0.CO%3B2).
- Barth, T. J., and Jespersen, D. C. (1989). The design and application of upwind schemes on unstructured meshes. *27th Aerospace Sciences Meeting*. [doi:10.2514/6.1989-366](https://doi.org/10.2514/6.1989-366).
