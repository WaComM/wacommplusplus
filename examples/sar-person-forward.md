# Forward person-in-water drift

## Scientific objective

Scientific question: given a reported loss position and time, where does a person in water move under the supplied ROMS current and a uniform 5 m s-1 eastward 10 m wind?

## Prerequisites, configuration, and command

Prerequisites are a processed `forcing.nc` with the ROMS fields documented in the adapter guide and a `sources.json` release definition. Replace both paths as needed. Configure and build with `cmake -S . -B build && cmake --build build`, then run `./build/wacommplusplus examples/sar-person-forward.json` from the repository root.

## Expected behavior and validation

The run emits source particles, adds right-of-downwind person-in-water leeway to the interpolated current, and writes output rooted at `sar-person-forward`. Validate first with `physics.random=false`: zero wind must match passive transport, and constant-field displacement must match the equation in `docs/sar-drift.md` within the grid/interpolation tolerance.

## Limitations, interpretation, and reproducibility

This example uses spatially uniform wind and deterministic mean coefficients; it does not represent wind uncertainty, jibing, Stokes drift, survival, or operational search planning. Interpret the result as a modeled future trajectory under those assumptions. Record the Git revision, complete JSON, forcing and source checksums, compiler/dependencies, CMake/backend options, platform, particle/thread/rank counts, numerical tolerances, and output checksums.


## References

- Dagestad, K.-F., Röhrs, J., Breivik, Ø., and Ådlandsvik, B. (2018). OpenDrift v1.0: a generic framework for trajectory modelling. *Geoscientific Model Development*, 11, 1405–1420. [doi:10.5194/gmd-11-1405-2018](https://doi.org/10.5194/gmd-11-1405-2018).
- Breivik, Ø., Allen, A. A., Maisondieu, C., and Roth, J.-C. (2011). Wind-induced drift of objects at sea: the leeway field method. *Applied Ocean Research*, 33, 100–109. [doi:10.1016/j.apor.2011.01.005](https://doi.org/10.1016/j.apor.2011.01.005).
