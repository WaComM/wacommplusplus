# Backward restart example

## Scientific objective

Continue deterministic backtracking from a physical checkpoint and compare it with uninterrupted backward integration.

## Prerequisites and configuration

Provide chronological ROMS forcing and a versioned `checkpoint-backward.nc` whose direction metadata is backward. Sources and diffusion are disabled so the comparison isolates reverse deterministic transport.

## Run and expected behavior

Build with `cmake -S . -B build && cmake --build build`, then run `./build/wacommplusplus examples/restart-backward.json`. The solver traverses files and records from newer to older, skipping intervals newer than the checkpoint and clipping the containing interval.

## Validation, limitations, and interpretation

Compare final particle identity, position, health, age, count, and checksums against an uninterrupted backward run. Boundary interactions and floating-point roundoff can make a forward/backward round trip non-exact; publish the tolerance used.

## Reproducibility

Archive revision, configuration, forcing/restart checksums, seed, compiler/dependencies, CMake options, platform/backend settings, tests, tolerances, and output checksums. See [backtracking](../docs/backtracking.md) and [restart](../docs/restart.md).


## References

- Dagestad, K.-F., Röhrs, J., Breivik, Ø., and Ådlandsvik, B. (2018). OpenDrift v1.0: a generic framework for trajectory modelling. *Geoscientific Model Development*, 11, 1405–1420. [doi:10.5194/gmd-11-1405-2018](https://doi.org/10.5194/gmd-11-1405-2018).
- Thygesen, U. H. (2011). How to reverse time in stochastic particle tracking models. *Journal of Marine Systems*, 88, 159–168. [doi:10.1016/j.jmarsys.2011.03.009](https://doi.org/10.1016/j.jmarsys.2011.03.009).
