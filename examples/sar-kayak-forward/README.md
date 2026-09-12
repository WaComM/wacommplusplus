# Forward kayak-with-person drift

## Scientific objective

Scientific question: where does a sea kayak with a person on its aft deck move under the supplied ROMS current and a uniform 5 m s-1 eastward 10 m wind when represented by the catalog-mean `PERSON-POWERED-VESSEL-1` leeway regression?

## Prerequisites, required fields, configuration, and command

Provide `forcing.nc` with the ROMS coordinates, mask, physical time, and eastward/northward current components in the units required by [the adapter guide](../../docs/adapters.md), plus a `sources.json` release inside a wet cell. The JSON selects deterministic forward tracking, disables turbulent random walk, selects `KAYAK_WITH_PERSON` on the right crosswind side, and supplies constant Earth-relative wind in m s-1. Build and run from the repository root with `cmake -S . -B build && cmake --build build && ./build/wacommplusplus examples/sar-kayak-forward/sar-kayak-forward.json`.

## Expected outputs and verification

The run writes snapshots rooted at `sar-kayak-forward`. In zero current, the catalog equation gives downwind speed `0.0116*5 + 0.1112 = 0.1692 m s-1` and right-crosswind speed `0.0041*5 = 0.0205 m s-1`. Verify displacement against those components after applying the grid metric, time step, and documented interpolation tolerance. Also run `ctest --test-dir build -R 'drift_model|configuration_round_trip|particle_physical_interval' --output-on-failure`.

## Limitations, interpretation, and reproducibility metadata

The coefficient class describes a specific experimental distress configuration and is not a universal kayak model. Constant wind omits meteorological structure; the run also omits coefficient uncertainty, wind error, waves, jibing, classification uncertainty, and operational search-area inference. Interpret output as a deterministic sensitivity trajectory conditional on the class and forcing. Archive the Git revision, complete configuration, forcing/source checksums, object source key, compiler and dependencies, CMake options, platform/backend layout, tolerances, and output checksums.

## References

- Breivik, Ø., Allen, A. A., Maisondieu, C., and Roth, J.-C. (2011). Wind-induced drift of objects at sea: the leeway field method. *Applied Ocean Research*, 33, 100--109. [doi:10.1016/j.apor.2011.01.005](https://doi.org/10.1016/j.apor.2011.01.005).
- Dagestad, K.-F., Röhrs, J., Breivik, Ø., and Ådlandsvik, B. (2018). OpenDrift v1.0: a generic framework for trajectory modelling. *Geoscientific Model Development*, 11, 1405--1420. [doi:10.5194/gmd-11-1405-2018](https://doi.org/10.5194/gmd-11-1405-2018).
