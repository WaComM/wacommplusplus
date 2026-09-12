# Backward person-in-water drift

## Scientific objective

Scientific question: from a known person-in-water endpoint, what prior location follows from deterministic backward integration of the supplied current and uniform wind leeway?

## Prerequisites, configuration, and command

Prerequisites are a processed ROMS `forcing.nc` and a version 3 `sar-endpoint.nc` containing object type and crosswind side. Configure and build with `cmake -S . -B build && cmake --build build`, then run `./build/wacommplusplus examples/sar-person-backward/sar-person-backward.json` from the repository root.

## Expected behavior and validation

The solver traverses forcing backward, suppresses ordinary sources, and applies its single negative `trackingDirection` to current plus leeway. Output is rooted at `sar-person-backward`. Validate with a paired deterministic forward constant-field case: the backward run should return to the initial position within the documented numerical tolerance and a split restart should match an uninterrupted run.

## Limitations, interpretation, and reproducibility

This is deterministic model backtracking, not proof of a unique true origin. Stochastic backward ensembles are probabilistic and are not exact inversions. The constant wind provider omits space/time variability, coefficient uncertainty, jibing, Stokes drift, and refloating. Record the Git revision, complete JSON, forcing and restart checksums, seed, compiler/dependencies, CMake/backend options, platform, rank/thread counts, tolerances, and output checksums.


## References

- Dagestad, K.-F., Röhrs, J., Breivik, Ø., and Ådlandsvik, B. (2018). OpenDrift v1.0: a generic framework for trajectory modelling. *Geoscientific Model Development*, 11, 1405–1420. [doi:10.5194/gmd-11-1405-2018](https://doi.org/10.5194/gmd-11-1405-2018).
- Breivik, Ø., Allen, A. A., Maisondieu, C., and Roth, J.-C. (2011). Wind-induced drift of objects at sea: the leeway field method. *Applied Ocean Research*, 33, 100–109. [doi:10.1016/j.apor.2011.01.005](https://doi.org/10.1016/j.apor.2011.01.005).
