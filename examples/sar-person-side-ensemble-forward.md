# Person-in-water crosswind-side ensemble: forward

## Scientific question

How does explicitly declared uncertainty in the initial left/right crosswind orientation alter forward person-in-water trajectories under a prescribed circulation and constant 10 m wind?

## Prerequisites, fields, and configuration

Provide chronological ROMS `forcing.nc` and `sources.json` inputs as specified in [the adapter guide](../docs/adapters.md). Current velocity must be in m s-1 and geographic coordinates in degrees. The configuration selects mean leeway coefficients, disables turbulent diffusion, and declares a Bernoulli prior with `side_right_probability=0.5`. This is a modeling prior, not a frequency inferred from the forcing or catalog. Run `cmake -S . -B build && cmake --build build && ./build/wacommplusplus examples/sar-person-side-ensemble-forward.json` from the repository root.

## Expected output and verification

Each stable particle identity receives one right-side assignment when its keyed uniform variate is below 0.5 and otherwise receives the left side. The resolved side is written to output/restart particle state and remains fixed. Repeating with the same seed and identities must reproduce assignments and trajectories exactly across serial, OpenMP, MPI, and CUDA scheduling. Verify both side values over a sufficiently large diagnostic ensemble, then check each constant-field displacement against the leeway equation in [the surface-drift guide](../docs/sar-drift.md) within the documented grid-metric tolerance.

## Limitations, interpretation, and reproducibility

The two orientations are conditional trajectory hypotheses. Equal prior mass is user-declared and is not an observational calibration, posterior probability, confidence statement, or search-area estimate. This example omits coefficient, forcing, object-class, release-position, jibing, and shoreline uncertainty. Archive the Git revision, complete configuration, source/forcing checksums, seed and stable identities, resolved side values, compiler/dependencies, CMake/backend options, platform layout, tolerances, and output checksums.

## References

- Breivik, Ø., Allen, A. A., Maisondieu, C., and Roth, J.-C. (2011). Wind-induced drift of objects at sea: the leeway field method. *Applied Ocean Research*, 33, 100–109. [doi:10.1016/j.apor.2011.01.005](https://doi.org/10.1016/j.apor.2011.01.005).
- Dagestad, K.-F., Röhrs, J., Breivik, Ø., & Ådlandsvik, B. (2018). OpenDrift v1.0: a generic framework for trajectory modelling. *Geoscientific Model Development*, 11, 1405–1420. [doi:10.5194/gmd-11-1405-2018](https://doi.org/10.5194/gmd-11-1405-2018).
