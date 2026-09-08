# Testing

Run the dependency-light numerical suite with the commands in [build.md](build.md). It covers interpolation bounds, vertical storage mapping, physical-time weights, shortened forward/backward steps, forward/backward restart clipping, reflection, and deterministic counter-key random values. Runtime-generated NEMO and HYCOM fixtures cover aliases, dimensions, normalized velocity, longitude conversion, and missing W/AKT fallbacks.

Application and backend validation additionally requires NetCDF fixtures and the relevant OpenMP, MPI, or CUDA runtime. Compare particle count, identity, position, health, and age with declared absolute and relative tolerances. Platform CI is build evidence, not proof of scientific equivalence.
