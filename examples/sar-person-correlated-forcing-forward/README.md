# Correlated leeway and wind uncertainty: forward

## Scientific objective

How do a declared covariance between downwind and crosswind leeway residuals and unresolved 10 m wind error alter a forward person-in-water ensemble?

## Prerequisites, fields, configuration, and command

Provide `forcing.nc` with the ROMS fields and SI units in `docs/adapters.md` and `sources.json` with multiple stable particle identities. Build with `cmake -S . -B build && cmake --build build`, then run `./build/wacommplusplus examples/sar-person-correlated-forcing-forward/sar-person-correlated-forcing-forward.json`. The illustrative configuration sets residual correlation to -0.35, wind-component standard deviation to 1.5 m s-1, east/north correlation to 0.4, spatial support to 10 km, and temporal support to 1 h; these are sensitivity assumptions, not calibrated statistics.

## Expected output and verification

The run writes NetCDF output rooted at `sar-person-correlated-forcing-forward`. Fixed correlated leeway residuals differ by particle identity, while particles in the same 10 km spatial bin and 1 h physical-time bin share a wind-error draw. Repetition with the same seed, identities, forcing, and bin definitions must be exact; a checkpoint at a completed substep must match an uninterrupted run. Setting all wind-correlation parameters to zero recovers the legacy identity/substep realization exactly.

## Limitations, interpretation, and reproducibility

The wind process is a discontinuous block field: covariance is perfect within a common configured bin and zero across its boundary. The equirectangular metre bins are inappropriate near poles or across the antimeridian, and support widths are not e-folding lengths. It omits current, wave, model-structure, object-class, and initial-condition uncertainty. The ensemble is a sensitivity experiment and is neither an observationally calibrated posterior nor a search-area probability. Archive the configuration and its scientific rationale, seed, identities, input and output checksums, Git revision, toolchain, backend layout, and tolerances.

## References

- Coppini, G., Jansen, E., Turrisi, G., Creti, S., Shchekinova, E. Y., Pinardi, N., Lecci, R., Carluccio, I., Kumkar, Y. V., D'Anca, A., Mannarini, G., Martinelli, S., Marra, P., Capodiferro, T., and Gismondi, T. (2016). A new search-and-rescue service in the Mediterranean Sea: a demonstration of the operational capability and an evaluation of its performance using real case scenarios. *Natural Hazards and Earth System Sciences*, 16, 2713–2727. [doi:10.5194/nhess-16-2713-2016](https://doi.org/10.5194/nhess-16-2713-2016).
- Breivik, Ø., and Allen, A. A. (2008). An operational search and rescue model for the Norwegian Sea and the North Sea. *Journal of Marine Systems*, 69, 99–113. [doi:10.1016/j.jmarsys.2007.02.010](https://doi.org/10.1016/j.jmarsys.2007.02.010).
