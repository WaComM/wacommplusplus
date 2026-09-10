# Backtracking

Backward mode traverses input files and forcing records from newest to oldest, reverses deterministic advection and terminal velocity, and suppresses ordinary forward source emission. Adapters remain direction-neutral.

Use endpoint particles from a restart, set `tracking.direction` to `backward`, and normally set `physics.random` to `false` and `tracking.backward_diffusion` to `none`. Validate a deterministic case by integrating forward and then backward with closures inactive; compare starting and recovered particle state within a stated tolerance.

`symmetric_stochastic` produces probabilistic candidate origins conditional on the forcing product and model assumptions. It is not the exact source, a unique origin, or reconstruction of a random forward path.

For drift objects, the deterministic vector reversed by the solver is the complete `V_current + V_leeway` velocity. Leeway coefficients and wind components are not negated inside the object model; the persisted side selects the same side-specific catalog regression in both directions. Consequently, a constant-field forward/backward round trip evaluates the same physical model in opposite temporal orientations. A fixed correlated coefficient-ensemble member or resolved random-side member is also reversible when wind error and jibing are disabled and seed and identity match. Wind-error and jibing draws use physical-interval/substep keys but do not invert a forward event history; their backward result is a family of conditional candidate origins. Stochastic diffusion, decay, unresolved forcing error, shoreline interaction, and jibing preclude exact pathwise reversibility.

## References

- Thygesen, U. H. (2011). How to reverse time in stochastic particle tracking models. *Journal of Marine Systems*, 88, 159–168. [doi:10.1016/j.jmarsys.2011.03.009](https://doi.org/10.1016/j.jmarsys.2011.03.009).
- Dagestad, K.-F., Röhrs, J., Breivik, Ø., and Ådlandsvik, B. (2018). OpenDrift v1.0: a generic framework for trajectory modelling. *Geoscientific Model Development*, 11, 1405–1420. [doi:10.5194/gmd-11-1405-2018](https://doi.org/10.5194/gmd-11-1405-2018).
