# Deterministic backward native-WACOMM example

## Scientific objective

Estimate deterministic candidate origins from known endpoints using normalized native WACOMM forcing.

## Prerequisites and required fields

Build with NetCDF C++4, provide chronological `wacomm.nc`, and provide a versioned direction-compatible `endpoint.nc`. Required native fields are listed in the forward guide. Ordinary sources are disabled.

## Configuration and run

The JSON selects `WACOMM`, `direction:backward`, `backward_diffusion:none`, and deterministic restart continuation. Replace paths, build, then run `./build/wacommplusplus examples/backward-wacomm/backward-wacomm.json`.

## Expected behavior and validation

The adapter returns identical normalized fields regardless of direction; the solver traverses records backward and reverses resolved and terminal velocity displacement. Check physical output time, identity preservation, bounds, and restart metadata. Run the native, particle-interval, and restart CTests, including the diffusion-free round trip and checkpoint continuation.

## Limitations, interpretation, and reproducibility

Decay, closures, and discretization can break exact reversibility. Describe output as candidate origins conditional on forcing and assumptions. Archive revision, configuration, forcing/restart checksums, seed, toolchain, dependencies, platform/backend settings, tolerances, tests, and outputs.


## References

- Dagestad, K.-F., Röhrs, J., Breivik, Ø., and Ådlandsvik, B. (2018). OpenDrift v1.0: a generic framework for trajectory modelling. *Geoscientific Model Development*, 11, 1405–1420. [doi:10.5194/gmd-11-1405-2018](https://doi.org/10.5194/gmd-11-1405-2018).
- Thygesen, U. H. (2011). How to reverse time in stochastic particle tracking models. *Journal of Marine Systems*, 88, 159–168. [doi:10.1016/j.jmarsys.2011.03.009](https://doi.org/10.1016/j.jmarsys.2011.03.009).
