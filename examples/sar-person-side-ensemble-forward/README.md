# Person-in-water crosswind-side ensemble: forward

## Scientific question

How does explicitly declared uncertainty in the initial left/right crosswind orientation alter forward person-in-water trajectories under a prescribed circulation and constant 10 m wind?

## Prerequisites, fields, and configuration

Provide chronological ROMS `forcing.nc` and `sources.json` inputs as specified in [the adapter guide](../../docs/adapters.md). Current velocity must be in m s-1 and geographic coordinates in degrees. The configuration selects mean leeway coefficients, disables turbulent diffusion, declares a Bernoulli initial-side prior of 0.5, and sets an illustrative hourly jibing probability of 0.04. Neither probability is inferred from the forcing or object catalog. Run `cmake -S . -B build && cmake --build build && ./build/wacommplusplus examples/sar-person-side-ensemble-forward/sar-person-side-ensemble-forward.json` from the repository root.

## Expected output and verification

Each stable identity receives one keyed initial side. At every completed substep, the hourly probability is converted to its exponential-hazard step probability and a disjoint keyed draw may flip that side. The current resolved value is written to output/restart state. Repeating with the same seed, identities, `dti`, and forcing partition must reproduce assignments, transitions, and trajectories across serial, OpenMP, MPI, and CUDA scheduling. Verify the limiting `jibe_probability_per_hour=0` case against fixed-side leeway and verify restart continuation at a completed substep boundary.

## Limitations, interpretation, and reproducibility

The initial 0.5 prior and hourly 0.04 transition probability are user-declared illustrative assumptions, not observational calibration, posterior probability, confidence statement, or search-area estimate. The constant hazard omits environmental or object-state dependence, permits repeated transitions across steps, and resolves at most one transition within a substep. This example omits coefficient, forcing, object-class, release-position, and shoreline uncertainty. Archive the Git revision, complete configuration, source/forcing checksums, seed and stable identities, `dti`, resolved side values, compiler/dependencies, CMake/backend options, platform layout, tolerances, and output checksums.

## References

- Breivik, Ø., Allen, A. A., Maisondieu, C., and Roth, J.-C. (2011). Wind-induced drift of objects at sea: the leeway field method. *Applied Ocean Research*, 33, 100–109. [doi:10.1016/j.apor.2011.01.005](https://doi.org/10.1016/j.apor.2011.01.005).
- Breivik, Ø., and Allen, A. A. (2008). An operational search and rescue model for the Norwegian Sea and the North Sea. *Journal of Marine Systems*, 69, 99–113. [doi:10.1016/j.jmarsys.2007.02.010](https://doi.org/10.1016/j.jmarsys.2007.02.010).
- Dagestad, K.-F., Röhrs, J., Breivik, Ø., & Ådlandsvik, B. (2018). OpenDrift v1.0: a generic framework for trajectory modelling. *Geoscientific Model Development*, 11, 1405–1420. [doi:10.5194/gmd-11-1405-2018](https://doi.org/10.5194/gmd-11-1405-2018).
