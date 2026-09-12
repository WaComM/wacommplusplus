# Deterministic backward HYCOM example

## Scientific objective

Trace known endpoints backward through HYCOM forcing to produce candidate deterministic origins conditional on the circulation product.

## Prerequisites and required fields

Build with NetCDF C++4, provide chronological `hycom.nc`, and provide versioned `endpoint.nc`. Required aliases and optional-field policy are detailed in [adapters](../../docs/adapters.md). Do not configure ordinary emission sources.

## Configuration and run

The JSON uses deterministic backward motion and `backward_diffusion:none`. Replace paths, run `cmake -S . -B build && cmake --build build`, then `./build/wacommplusplus examples/backward-hycom/backward-hycom.json`.

## Expected behavior and validation

HYCOM data remains chronological and direction-neutral while the solver traverses newer to older records. Check restart direction safety, identities, time ordering, and bounds. Compare a deterministic forward/backward fixture within tolerance and run the structured-adapter, particle-interval, and restart tests.

## Limitations, interpretation, and reproducibility

Boundary interactions, decay, coarse forcing, missing W/AKT, and numerical error can prevent reversibility. Results are candidate origins, not a unique history. Archive revision, configuration, forcing/restart checksums, seed, toolchain, dependencies, platform/backend settings, tolerances, tests, and outputs.


## References

- Dagestad, K.-F., Röhrs, J., Breivik, Ø., and Ådlandsvik, B. (2018). OpenDrift v1.0: a generic framework for trajectory modelling. *Geoscientific Model Development*, 11, 1405–1420. [doi:10.5194/gmd-11-1405-2018](https://doi.org/10.5194/gmd-11-1405-2018).
- Thygesen, U. H. (2011). How to reverse time in stochastic particle tracking models. *Journal of Marine Systems*, 88, 159–168. [doi:10.1016/j.jmarsys.2011.03.009](https://doi.org/10.1016/j.jmarsys.2011.03.009).
