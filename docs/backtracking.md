# Backtracking

Backward mode traverses input files and forcing records from newest to oldest, reverses deterministic advection and terminal velocity, and suppresses ordinary forward source emission. Adapters remain direction-neutral.

Use endpoint particles from a restart, set `tracking.direction` to `backward`, and normally set `physics.random` to `false` and `tracking.backward_diffusion` to `none`. Validate a deterministic case by integrating forward and then backward with closures inactive; compare starting and recovered particle state within a stated tolerance.

`symmetric_stochastic` produces probabilistic candidate origins conditional on the forcing product and model assumptions. It is not the exact source, a unique origin, or reconstruction of a random forward path.
