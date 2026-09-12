# Stochastic backward ensemble example

## Scientific objective

Explore candidate origins for known endpoints under a symmetric stochastic backward formulation. The ensemble supports probabilistic source attribution; it does not reconstruct a unique past random path.

## Prerequisites and input

Provide chronologically ordered ROMS forcing files and a versioned backward-compatible `endpoints.nc` restart containing the known endpoint particles. Required adapter variables are listed in [adapters](../../docs/adapters.md). Ordinary emission sources are disabled.

## Run and expected behavior

Build with `cmake -S . -B build && cmake --build build`, then run `./build/wacommplusplus examples/stochastic-backward-ensemble/stochastic-backward-ensemble.json`. The solver traverses forcing from newer to older times, reverses deterministic displacement, and applies the explicitly selected symmetric stochastic ensemble.

## Validation, limitations, and interpretation

Repeat with seed `5489` and compare outputs, then change the seed and compare the origin distribution. Verify that restart direction metadata is `backward`. Conclusions are conditional on circulation, diffusion, boundaries, endpoint uncertainty, and ensemble size; report candidate origins rather than an exact source.

## Reproducibility

Record revision, configuration, forcing/restart checksums, seed, toolchain, dependencies, platform, backend settings, tolerances, tests, ensemble size, and output checksums. See [backtracking](../../docs/backtracking.md), [restart](../../docs/restart.md), and [references](../../docs/references.md).


## References

- Dagestad, K.-F., Röhrs, J., Breivik, Ø., and Ådlandsvik, B. (2018). OpenDrift v1.0: a generic framework for trajectory modelling. *Geoscientific Model Development*, 11, 1405–1420. [doi:10.5194/gmd-11-1405-2018](https://doi.org/10.5194/gmd-11-1405-2018).
- Thygesen, U. H. (2011). How to reverse time in stochastic particle tracking models. *Journal of Marine Systems*, 88, 159–168. [doi:10.1016/j.jmarsys.2011.03.009](https://doi.org/10.1016/j.jmarsys.2011.03.009).
