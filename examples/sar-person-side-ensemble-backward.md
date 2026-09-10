# Person-in-water crosswind-side ensemble: backward

## Scientific question

How does an explicitly declared prior over left/right crosswind orientation affect the family of modeled prior locations reconstructed from person-in-water endpoints?

## Prerequisites, fields, and configuration

Provide chronological ROMS `forcing.nc` spanning the reconstruction interval and a `sources.json` whose releases represent endpoint particles with stable identities. Required fields and units are defined in [the adapter guide](../docs/adapters.md). The run disables diffusion and coefficient residuals, declares an initial right-side probability of 0.5, and supplies an illustrative hourly jibing probability of 0.04. Build and execute with `cmake -S . -B build && cmake --build build && ./build/wacommplusplus examples/sar-person-side-ensemble-backward.json`.

## Expected output and verification

The same seed and identity produce reproducible initial-side and substep-transition draws. The hazard uses absolute physical duration while the solver alone determines temporal orientation. With jibing set to zero, a constant-field forward/backward pair recovers its initial point within tolerance. With jibing enabled, backward paths are stochastic candidate histories and are not exact inverses. A run split at a completed substep must equal uninterrupted backward execution when its checkpoint begins from the stored resolved side.

## Limitations, interpretation, and reproducibility

The output is a conditional sensitivity ensemble, not a unique inverse trajectory, calibrated posterior, confidence region, or operational search area. Both probabilities are explicit experiment assumptions. The constant-hazard law omits dependence on sea state, object attitude, or elapsed state duration and resolves at most one transition per substep. Archive revision, resolved configuration, endpoint/forcing/restart checksums, seed, stable identities, `dti`, resolved sides, compiler/dependencies, CMake/backend options, execution layout, tolerances, and output checksums.

## References

- Breivik, Ø., Allen, A. A., Maisondieu, C., and Roth, J.-C. (2011). Wind-induced drift of objects at sea: the leeway field method. *Applied Ocean Research*, 33, 100–109. [doi:10.1016/j.apor.2011.01.005](https://doi.org/10.1016/j.apor.2011.01.005).
- Breivik, Ø., and Allen, A. A. (2008). An operational search and rescue model for the Norwegian Sea and the North Sea. *Journal of Marine Systems*, 69, 99–113. [doi:10.1016/j.jmarsys.2007.02.010](https://doi.org/10.1016/j.jmarsys.2007.02.010).
- Dagestad, K.-F., Röhrs, J., Breivik, Ø., & Ådlandsvik, B. (2018). OpenDrift v1.0: a generic framework for trajectory modelling. *Geoscientific Model Development*, 11, 1405–1420. [doi:10.5194/gmd-11-1405-2018](https://doi.org/10.5194/gmd-11-1405-2018).
