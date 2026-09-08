# WaComM++ engineering and scientific contract

There is one WaComM++ physical and numerical model. Serial, OpenMP, MPI, FlexMPI/EMPI, CUDA, OpenACC, forward, backward, and restart execution must not silently implement different equations.

Changes to advection, diffusion, interpolation, grid metrics, vertical coordinates, boundaries, decay, sources, settling or rise velocity, temporal integration, or stochastic methods must include a mathematical description and units, scientific references where applicable, regression and backend-equivalence tests, forward/backward and restart analysis, and documentation and example updates.

The source-level model follows the existing execution hierarchy:

```text
ocean forcing/time progression
    -> MPI/FlexMPI distributed-memory decomposition
        -> OpenMP shared-memory particle parallelism
            -> CUDA/accelerator execution where enabled
```

No accelerator or backend may contain a separate physical model. ROMS, NEMO, HYCOM, and native WaComM adapters normalize physical data only. They must not negate velocities, reverse `ocean_time`, reverse file order, or select diffusion from tracking direction. Direction belongs to the solver.

All changes must preserve the original WaComM++ repository coding style, including the style, tone, structure, and granularity of existing comments. Contributors and coding agents must not mass-reformat unrelated code or introduce a competing style inside existing source files. Follow nearby code, preserve established terminology, and rewrite legacy comments only when they are incorrect or directly affected.

Restart behavior is defined in physical time and must satisfy, within declared tolerances:

```text
continuous forward == restarted forward
continuous backward == restarted backward
```

Documentation and examples are tested interfaces. User-visible changes to defaults, options, equations, adapters, platforms, restart or tracking behavior, fields, and build options require corresponding documentation and example changes.

Every user-visible feature requires a documented example. Coverage includes ROMS, NEMO, HYCOM, native WaComM, forward and backward tracking, deterministic and stochastic modes, forward and backward restart, OpenMP, MPI, and CUDA where supported. Each guide describes its scientific question, prerequisites, fields, configuration, exact command, expected behavior, validation, limitations, interpretation, and reproducibility metadata.

The portable C++17 core remains buildable without optional HPC backends on Ubuntu Linux x86_64, Rocky Linux 8.9 x86_64, macOS Intel and Apple Silicon, Windows/MSVC x86_64, Raspberry Pi OS 64-bit ARM64, and RISC-V 64 Linux. Do not make Fortran, MPI, OpenMP, CUDA, OpenACC, or EMPI mandatory for configuring the portable core.

Reproducible scientific runs record the Git revision, complete configuration, forcing and restart checksums, configured random seed, compiler, CMake options, dependencies, platform, parallel execution settings, numerical tolerances, and output checksums. Production stochastic runs must never use wall-clock, rank, thread, or accelerator scheduling as their scientific seed.

WaComM++ is one versioned scientific product comprising physics, implementation, tests, examples, and documentation. Review releases across all five layers.
