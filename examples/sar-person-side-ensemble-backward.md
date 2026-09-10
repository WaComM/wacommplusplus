# Person-in-water crosswind-side ensemble: backward

## Scientific question

How does an explicitly declared prior over left/right crosswind orientation affect the family of modeled prior locations reconstructed from person-in-water endpoints?

## Prerequisites, fields, and configuration

Provide chronological ROMS `forcing.nc` spanning the reconstruction interval and a `sources.json` whose releases represent endpoint particles with stable identities. Required fields and units are defined in [the adapter guide](../docs/adapters.md). The run disables diffusion and coefficient residuals, and declares `side_right_probability=0.5`. Build and execute with `cmake -S . -B build && cmake --build build && ./build/wacommplusplus examples/sar-person-side-ensemble-backward.json`.

## Expected output and verification

The same seed and identity produce the same fixed side as in forward execution; only the solver reverses temporal orientation. Under constant reversible fields, forward integration followed by backward integration with the same identity, seed, resolved side, and duration must recover the initial position within the declared numerical tolerance. Restart output must preserve the resolved `drift_side` rather than resampling it.

## Limitations, interpretation, and reproducibility

The output is a conditional sensitivity ensemble, not a unique inverse trajectory, calibrated posterior, confidence region, or operational search area. The 0.5 prior is an explicit experiment assumption. Jibing is absent, so a member cannot switch sides during its trajectory. Archive revision, resolved configuration, endpoint/forcing/restart checksums, seed, stable identities and resolved sides, compiler/dependencies, CMake/backend options, execution layout, tolerances, and output checksums.

## References

- Breivik, Ø., Allen, A. A., Maisondieu, C., and Roth, J.-C. (2011). Wind-induced drift of objects at sea: the leeway field method. *Applied Ocean Research*, 33, 100–109. [doi:10.1016/j.apor.2011.01.005](https://doi.org/10.1016/j.apor.2011.01.005).
- Dagestad, K.-F., Röhrs, J., Breivik, Ø., & Ådlandsvik, B. (2018). OpenDrift v1.0: a generic framework for trajectory modelling. *Geoscientific Model Development*, 11, 1405–1420. [doi:10.5194/gmd-11-1405-2018](https://doi.org/10.5194/gmd-11-1405-2018).
