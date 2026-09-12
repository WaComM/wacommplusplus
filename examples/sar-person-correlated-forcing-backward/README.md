# Correlated leeway and wind uncertainty: backward

## Scientific objective

How does the same declared leeway covariance and unresolved-wind model generate reproducible candidate origins when the particle solver traverses physical forcing backward?

## Prerequisites, fields, configuration, and command

Provide the ROMS `forcing.nc` and an endpoint `sources.json` described by the adapter and source guides. Build with `cmake -S . -B build && cmake --build build`, then run `./build/wacommplusplus examples/sar-person-correlated-forcing-backward/sar-person-correlated-forcing-backward.json`. The -0.35 residual correlation, 1.5 m s-1 component standard deviation, 0.4 east/north correlation, 10 km spatial support, and 1 h temporal support are explicit sensitivity parameters requiring study-specific evidence.

## Expected output and verification

The run writes NetCDF output rooted at `sar-person-correlated-forcing-backward`. The solver reverses the integration increment but never negates wind or leeway inside an adapter. Same-seed repetition and continuation at completed substeps must match exactly across serial, OpenMP, and MPI scheduling. Zero wind uncertainty with fixed residuals permits a constant-field forward/backward regression; nonzero temporally resampled wind error is not pathwise reversible.

## Limitations, interpretation, and reproducibility

Each member is a conditional candidate-origin history, not the unique past path. The Gaussian block wind-error model is not a meteorological ensemble and carries no calibrated coverage probability. Its covariance is perfect inside common space/time bins and zero across boundaries; equirectangular bins are unsuitable near poles and the antimeridian. It excludes current and wave error, object misclassification, and observation error. Archive parameter provenance, seed, identities, configuration, forcing/restart/output checksums, revision, dependencies, backend settings, and tolerances.

## References

- Coppini, G., Jansen, E., Turrisi, G., Creti, S., Shchekinova, E. Y., Pinardi, N., Lecci, R., Carluccio, I., Kumkar, Y. V., D'Anca, A., Mannarini, G., Martinelli, S., Marra, P., Capodiferro, T., and Gismondi, T. (2016). A new search-and-rescue service in the Mediterranean Sea: a demonstration of the operational capability and an evaluation of its performance using real case scenarios. *Natural Hazards and Earth System Sciences*, 16, 2713–2727. [doi:10.5194/nhess-16-2713-2016](https://doi.org/10.5194/nhess-16-2713-2016).
- Thygesen, U. H. (2011). How to reverse time in stochastic particle tracking models. *Journal of Marine Systems*, 88, 159–168. [doi:10.1016/j.jmarsys.2011.03.009](https://doi.org/10.1016/j.jmarsys.2011.03.009).
