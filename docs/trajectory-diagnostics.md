# Trajectory maps and ensemble diagnostics

WaComM++ separates numerical integration from diagnostic inference. `tools/trajectory_diagnostics.py` reads one or more particle-snapshot NetCDF files, joins observations through their stable 64-bit trajectory identifiers, orders snapshots by physical `particle_time`, and writes deterministic JSON diagnostics, a static SVG map, and optionally a self-contained interactive HTML view. The tool never changes particle state, forcing, or solver equations.

## Diagnostic definitions

For snapshot members indexed by `n`, the reported latitude centroid is the arithmetic mean of latitude. Longitude uses a circular mean to avoid a discontinuity at the antimeridian. For member position `(phi_n, lambda_n)` and centroid `(phi_c, lambda_c)`, radial distance in meters is the spherical great-circle approximation

$$
d_n=2R\arctan2\left(\sqrt{a_n},\sqrt{1-a_n}\right),\qquad
a_n=\sin^2\frac{\phi_n-\phi_c}{2}+\cos\phi_n\cos\phi_c\sin^2\frac{\lambda_n-\lambda_c}{2},
$$

where angles are radians and `R=6,371,000 m`. The JSON reports member count, geographic extent, centroid, median, 90th percentile, and maximum of `d_n` at every snapshot time. Quantiles use linear interpolation between ordered observations. These descriptive statistics are not a covariance model, probability density, confidence region, or validation score. The latitude/circular-longitude centroid is intended for regional ensembles; it is not a spherical Fréchet mean and should not be used for global or antipodal distributions.

By default, finite positions with positive `health` are selected. `--include-inactive` retains finite inactive members. Files without `health` are treated as containing selected positions. Required variables are `id`, `lat`, `lon`, and a single `particle_time` with units `seconds since 1968-05-23 00:00:00 GMT`; malformed or duplicate-time snapshots fail rather than being guessed.

## Map semantics

The SVG is a diagnostic Plate Carrée rendering in EPSG:4326. Its visible annotation and accessible description record longitude/latitude extent, physical time range, angular units, projection, absence of coastline/basemap data, and the statement that paths are ensemble members rather than probabilities. Longitude is unwrapped locally around the circular mean so antimeridian-crossing ensembles remain compact. The map is appropriate for trajectory inspection, not area-preserving measurement or navigation.

The optional HTML view embeds the selected coordinates and metadata directly and makes no network requests. A physical-time slider and play/pause control expose one recorded snapshot at a time; complete member paths can be hidden, and clicking or keyboard-activating a point highlights its stable trajectory identifier. A live table reports the selected time, member count, and the same arithmetic-latitude/circular-longitude centroid defined above. Playback changes presentation only: it does not interpolate positions, alter timestamps, or imply temporal resolution between snapshots. The view supports keyboard operation and light/dark color schemes, but remains an EPSG:4326 diagnostic without coastline context.

## Example workflow

After a run has produced chronologically distinct snapshots, execute:

```bash
python3 tools/trajectory_diagnostics.py output-000.nc output-060.nc output-120.nc \
  --json trajectory-diagnostics.json \
  --svg trajectory-map.svg \
  --html trajectory-interactive.html
```

The Python `netCDF4` package is required. `--html` is optional; the JSON and SVG remain required archival products. Input order is irrelevant because physical time controls ordering. Validate a workflow by permuting input order and requiring byte-identical results except when absolute input paths differ; test a coincident ensemble for zero radial spread, an analytical two-member separation, inactive-member selection, an antimeridian case, and the interactive controls and embedded uncertainty statement. Archive the tool revision, command, input/output checksums, generated JSON, SVG, HTML when used, and the provenance attributes copied from each NetCDF input.

## Limitations and interpretation

The tool does not interpolate missing times, infer connectivity when identifiers change, estimate probability contours, calculate search areas, download coastlines, or assert observational skill. A spatial envelope can reflect initial-condition, parameter, diffusion, and forcing choices simultaneously; attribution requires a controlled experimental design. Comparisons between forward and backward ensembles must retain their distinct conditional interpretations.

## References

- Dagestad, K.-F., Röhrs, J., Breivik, Ø., and Ådlandsvik, B. (2018). OpenDrift v1.0: a generic framework for trajectory modelling. *Geoscientific Model Development*, 11, 1405–1420. [doi:10.5194/gmd-11-1405-2018](https://doi.org/10.5194/gmd-11-1405-2018).
- Thygesen, U. H. (2011). How to reverse time in stochastic particle tracking models. *Journal of Marine Systems*, 88, 159–168. [doi:10.1016/j.jmarsys.2011.03.009](https://doi.org/10.1016/j.jmarsys.2011.03.009).
- Sandve, G. K., Nekrutenko, A., Taylor, J., and Hovig, E. (2013). Ten simple rules for reproducible computational research. *PLoS Computational Biology*, 9, e1003285. [doi:10.1371/journal.pcbi.1003285](https://doi.org/10.1371/journal.pcbi.1003285).
