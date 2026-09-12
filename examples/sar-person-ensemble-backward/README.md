# Backward person-in-water coefficient ensemble

## Scientific objective

Scientific question: from a known recovery location and time, what family of prior locations is obtained by reversing the deterministic transport of independently sampled person-in-water leeway members?

## Prerequisites, configuration, and command

Provide chronological ROMS `forcing.nc` data spanning the reconstruction interval and a `sources.json` whose releases represent endpoint particles with stable identities. Required current and grid variables and units are specified in `docs/adapters.md`. Build with `cmake -S . -B build && cmake --build build`, then run `./build/wacommplusplus examples/sar-person-ensemble-backward/sar-person-ensemble-backward.json`. The random walk is disabled; the configured seed is still required because it deterministically indexes leeway residuals.

## Expected behavior and validation

The solver applies negative temporal orientation to the complete current-plus-leeway velocity while retaining each particle's fixed residual pair. Repetition with unchanged seed and identities must be exact. In a constant-field verification, a forward member followed backward with the same identity, object, side, seed, and duration must return to its initial point within the declared floating-point and grid-metric tolerance.

## Limitations, interpretation, and reproducibility

The result is a sensitivity ensemble conditional on the empirical Gaussian residual model, not a Bayesian posterior or proof of a unique origin. It excludes forcing uncertainty, side uncertainty, jibing, turbulent diffusion, Stokes drift, and boundary irreversibility. Gaussian tails are unbounded. Archive the Git revision, complete JSON, endpoint/source and forcing checksums, seed and particle identities, compiler/dependencies, CMake/backend options, platform and parallel layout, tolerances, and output checksums.

## References

- Breivik, Ø., Allen, A. A., Maisondieu, C., and Roth, J.-C. (2011). Wind-induced drift of objects at sea: the leeway field method. *Applied Ocean Research*, 33, 100–109. [doi:10.1016/j.apor.2011.01.005](https://doi.org/10.1016/j.apor.2011.01.005).
- Thygesen, U. H. (2011). How to reverse time in stochastic particle tracking models. *Journal of Marine Systems*, 88, 159–168. [doi:10.1016/j.jmarsys.2011.03.009](https://doi.org/10.1016/j.jmarsys.2011.03.009).
