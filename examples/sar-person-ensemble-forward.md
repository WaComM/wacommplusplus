# Forward person-in-water coefficient ensemble

## Scientific objective

Scientific question: how does cataloged residual variability in person-in-water leeway alter the family of modeled future trajectories under a prescribed ROMS current and uniform 5 m s-1 eastward 10 m wind?

## Prerequisites, configuration, and command

Provide a processed `forcing.nc` with the ROMS fields and units documented in `docs/adapters.md` and a `sources.json` containing multiple released particles with stable identities. Build with `cmake -S . -B build && cmake --build build`, then run `./build/wacommplusplus examples/sar-person-ensemble-forward.json` from the repository root. The configuration disables turbulent random walk but enables `drift.coefficient_ensemble`; `physics.random_seed=5489` therefore controls the fixed empirical residual assigned to each particle.

## Expected behavior and validation

Particles sharing object class, side, forcing, and release state acquire distinct downwind and crosswind residuals when their identities differ. Repeating the run with the same seed and identities must reproduce positions exactly across serial and OpenMP scheduling. Setting `coefficient_ensemble=false` must recover the catalog-mean trajectory. Under constant fields, each member must agree with the equation in `docs/sar-drift.md` within the declared grid-metric tolerance.

## Limitations, interpretation, and reproducibility

This ensemble samples independent Gaussian regression residuals; it does not sample forcing error, object misclassification, crosswind-side uncertainty, jibing, coastline uncertainty, or Stokes drift. Members are conditional trajectory hypotheses, not calibrated probabilities or an operational search area. Gaussian residuals are unbounded and may reverse a component for an individual draw. Record the Git revision, complete JSON, source/forcing checksums, particle identities, seed, compiler and dependencies, CMake/backend options, platform and parallel layout, numerical tolerances, and output checksums.

## References

- Breivik, Ø., Allen, A. A., Maisondieu, C., and Roth, J.-C. (2011). Wind-induced drift of objects at sea: the leeway field method. *Applied Ocean Research*, 33, 100–109. [doi:10.1016/j.apor.2011.01.005](https://doi.org/10.1016/j.apor.2011.01.005).
- Dagestad, K.-F., Röhrs, J., Breivik, Ø., and Ådlandsvik, B. (2018). OpenDrift v1.0: a generic framework for trajectory modelling. *Geoscientific Model Development*, 11, 1405–1420. [doi:10.5194/gmd-11-1405-2018](https://doi.org/10.5194/gmd-11-1405-2018).
