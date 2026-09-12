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

## Publication maps and profiles

`tools/publication_figures.py` adds read-only map panels, vertical count profiles, a final radial section, and depth summaries. The [six-hour Sarno guide](../examples/wacomm-sarno-lite.md) gives the full command, actual results, limitations, and provenance. Its figures are [maps](figures/sarno-lite/sarno-maps.svg) and [profiles](figures/sarno-lite/sarno-profiles.svg). Install the optional pinned Python 3.11 dependencies with `pip install -r tools/requirements-figures.txt`; they are not portable-core dependencies. SVG/PDF preserve labels and contours as vectors, rasterize dense point layers, and provide white backgrounds legible in either reading theme. PNG uses 400 dpi. Root SVG titles/descriptions support accessibility.

The tool requires one `particle_time` per snapshot at the existing WaComM epoch and Gregorian calendar, integer `id`, geographic `lon`/`lat`, `depth` with explicit metre units and `positive=up|down`, `age` in seconds (including the writer's `seconds since emission`), and `health`. It preserves identifiers as integers, including values above 2^53, and rejects duplicate identifiers/times or mismatched dimensions/units. Masked, non-finite, sentinel-magnitude (absolute value at least 1e30), out-of-range geographic coordinates, and negative ages are excluded and counted. Only positive-health members enter the figures. Empty selected snapshots fail rather than disappear. No tracks are joined or extrapolated between saved times.

Depth is expressed positive down by negating stored positive-up values; stored positive-down data retain their sign. All estimators use metres internally. `--depth-unit mm` multiplies displayed depths by 1000 and rescales count-per-length axes consistently. WaComM's current depth writer interpolates the bathymetry/sigma coordinate without adding instantaneous sea level, so the axis is labeled **depth below model datum**, not observed instantaneous sea-surface depth.

For bin edges $b_j$ in metres and selected depths $d_i$ in metres, the vertical count density is

$$
c_j = \frac{1}{b_{j+1}-b_j}\sum_{i=1}^{N}\mathbf{1}_{[b_j,b_{j+1})}(d_i).
$$

Here $N$ is the selected member count (dimensionless), the indicator is one for membership and zero otherwise, and $c_j$ has units particles m⁻¹. The final bin includes its right edge. The common bins span all selected depths across snapshots with configured positive finite width `--depth-bin` in metres. Counts outside the bins are recorded; bins use no mass weighting, smoothing, probability normalization, or area/volume correction. Multiplying count densities by their bin widths recovers the selected counts. The map plots all selected members, ordered by stable ID, with age in hours as color; transparency/overplotting is not a density estimate.

Depth medians and 10th/90th percentiles use linear sample quantiles at each recorded time. These combine newly released and surviving members and are not fixed-cohort survival summaries or confidence bounds. The final radial section uses the spherical great-circle distance defined above, now referenced to the explicitly supplied source rather than a centroid, with metres converted to kilometres. It collapses azimuth and must not be interpreted as a signed alongshore/offshore transect. No projection-plane distance or spatial probability is inferred.

The explicit `--map-crs EPSG:4326` selects Plate Carrée with equal plotted angular scales. The explicit `--extent WEST EAST SOUTH NORTH` must be finite, within geographic bounds, non-polar, at most 10° wide/high, and must enclose every selected member and the source; antimeridian-crossing extents are rejected. This is a regional visualization, not area-preserving measurement. A locally supplied ROMS grid provides metre bathymetry and binary wet/dry masks. The tool reads those fields without changing them and crops a rectangular index stencil around the requested extent. Matplotlib contours interpolate within this grid solely for display: the 0.5 mask contour is **not a surveyed coastline or the exact solver shore criterion**. No external geography, tiles, or geographic inference from a filename is used. Cropped quadrilaterals must have consistently oriented corner cross products with magnitude greater than 1e-14 degree². Degenerate/mismatched grids, unknown units, invalid bathymetry, and non-binary masks are rejected. No conservation claim applies to rendering.

`sarno-figure-manifest.json` records selections, input hashes and provenance, depth bins, exact summaries, grid identity/hash, extent, software versions, script/helper hashes, and figure hashes. Inputs are opened read-only; paths that would overwrite an input are rejected. Reversed input-file order yields the same outputs in the pinned environment. Bitwise rendering across different Matplotlib/font/library versions is not promised. Tests exercise analytical histograms, zero source distance, positive-up conversion, invalid/masked members, 64-bit identities, duplicate rejection, unit validation, input-order invariance, accessible exports, and unchanged input hashes.

The OpenDrift [ROMS gallery](https://opendrift.github.io/gallery/example_roms_native.html) and [vertical mixing gallery](https://opendrift.github.io/gallery/example_vertical_mixing.html) informed the map-plus-profile presentation. The implementation reads WaComM output directly; these are not comparative OpenDrift runs or evidence of model equivalence. Scientific interpretation remains subject to the model assumptions and peer-reviewed discussion below.

## Exact particle snapshot comparison

`tools/compare_particle_snapshots.py REFERENCE CANDIDATE --json REPORT` uses the optional NumPy/NetCDF4 environment above. It compares all stored particles, including inactive members, by exact integer identity and physical time, without spatial projection or interpolation. Times must be finite singleton values with the native Gregorian epoch metadata. Duplicate IDs/times, different times, schemas, units, sign conventions, calendars, masks, or non-finite unmasked values fail. Masked values are excluded only when both masks agree. Every remaining variable value must be numerically equal (absolute and relative tolerances zero); identifiers remain integers beyond 2^53. Global attributes are excluded so build provenance can differ. This is a state-equivalence check, not a statistical estimator, file-byte comparison, or observational validation. It does not compare gridded output or native forcing. Inputs remain read-only, and the report cannot overwrite an input. See the [two-process Sarno example](../examples/wacomm-sarno-lite.md#two-process-mpi-calculation) for the exact command and evidence.

## Strong-scaling figures

`tools/scaling_figures.py data/wacomm-sarno-lite/scaling --output-dir docs/figures/sarno-lite/scaling` creates separate speedup and efficiency plots in SVG, PDF, and 400 dpi PNG, plus `scaling-results.json`. It requires the optional plotting dependencies. The full [example protocol](../examples/wacomm-sarno-lite.md#mpiopenmp-strong-scaling) specifies resources and workload. Let $p$ be the dimensionless MPI process count and $T_p>0$ the sum of five measured solver-interval durations in seconds with one OpenMP thread per process. The dimensionless estimators are

$$
S_p = T_1/T_p, \qquad E_p = S_p/p.
$$

The efficiency figure displays $100E_p$ in percent. Ideal reference lines are $S_p=p$ and $E_p=1$; they are theoretical comparisons, not fitted results (Amdahl, 1967). Rank zero records `Solver interval: start=... end=... seconds=...` using `steady_clock` with 17-digit text output. The MPI barrier before timing excludes forcing preparation/loading on every rank; the barrier after timing prevents overlap with the next forcing load. Timed work comprises source insertion, MPI scatter/gather, particle updates, concentration evaluation and masking; file writes and forcing are excluded. The collector requires exactly the physical intervals 09–10, 10–11, 11–13, 13–14, and 14–15 UTC, with positive finite durations, and sums them using `math.fsum`. Duplicate, missing, or reordered intervals fail. Timestamp/output precision is not an estimate of timer accuracy.

The secondary orange series independently normalizes full application elapsed time $A_p$ (seconds), using $A_1/A_p$ and $100A_1/(pA_p)$. GNU `time` measures `mpirun` at 0.01 s output resolution, including startup, native reads, calculation and writes. Downloading and one-process ROMS-to-native preparation finish in a separate job before any performance job; queueing, staging and checksums are excluded from both series. No interpolation, averaging, outlier rejection, or missing-run imputation is used. One sample per count and timing scope supports no variance or confidence interval; the shared baseline also makes the ratios statistically dependent. Both axes are logarithmic to show solver and full-application scaling alongside ideal scaling.

The collector requires the native WaComM adapter with `processed-6h/` and `save_input=false`, all six counts, positive finite times, successful application exits, identical executable/configuration/source hashes, and recorded one-thread settings. It verifies all stored particle states against the one-rank baseline using exact integer identity and physical-time matching. Missing or differing runs fail instead of yielding a partial plot. Inputs are read-only and the destination must be outside the simulation root. Global build metadata, gridded fields, and normalized forcing are outside the particle comparison. The report archives timing, Slurm and binding records, configuration, compiler/build cache, revision and working diff, forcing/output checksums, and figure hashes. Sequential ascending execution does not control filesystem cache, shared storage load, memory bandwidth, or system noise. Node exclusivity excludes competing Slurm allocations but is not a controlled cache experiment. These are computational diagnostics, not scientific validation of trajectories.

## References

- Dagestad, K.-F., Röhrs, J., Breivik, Ø., and Ådlandsvik, B. (2018). OpenDrift v1.0: a generic framework for trajectory modelling. *Geoscientific Model Development*, 11, 1405–1420. [doi:10.5194/gmd-11-1405-2018](https://doi.org/10.5194/gmd-11-1405-2018).
- Thygesen, U. H. (2011). How to reverse time in stochastic particle tracking models. *Journal of Marine Systems*, 88, 159–168. [doi:10.1016/j.jmarsys.2011.03.009](https://doi.org/10.1016/j.jmarsys.2011.03.009).
- Sandve, G. K., Nekrutenko, A., Taylor, J., and Hovig, E. (2013). Ten simple rules for reproducible computational research. *PLoS Computational Biology*, 9, e1003285. [doi:10.1371/journal.pcbi.1003285](https://doi.org/10.1371/journal.pcbi.1003285).

- Amdahl, G. M. (1967). Validity of the single processor approach to achieving large scale computing capabilities. *AFIPS Conference Proceedings*, 30 (Spring Joint Computer Conference), 483–485. [doi:10.1145/1465482.1465560](https://doi.org/10.1145/1465482.1465560).
