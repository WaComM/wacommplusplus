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

Documentation is a scientific interface and must remain semantically synchronized with the implementation. A change to equations, parameter values, units, sign conventions, configuration keys, defaults, environmental inputs, object catalogs, restart variables, output variables, adapters, execution backends, or validation tolerances must update the model description, configuration reference, relevant example guides, main README, and repository reference list in the same change.

Scientific narrative must meet the standard of a peer-reviewed computational modeling article. Distinguish governing equations, empirical parameterizations, numerical methods, software architecture, verification evidence, scientific validation evidence, assumptions, uncertainty, limitations, and interpretation. Never present numerical agreement as observational validation, a stochastic backward ensemble as a unique inverse trajectory, or a catalog coefficient as universally valid outside its documented experimental scope.

Every document that states model physics or describes scientific/runtime configuration must contain an explicit `References` section. Normative scientific claims must cite verified peer-reviewed journal articles or peer-reviewed conference proceedings with persistent identifiers where available. Technical reports, manuals, websites, source-code comments, and third-party data catalogs may document provenance or interoperability, but they must not replace peer-reviewed support for a physical or numerical claim. Verify author list, title, venue, year, volume/pages, and DOI before adding a citation.

Every checked-in example configuration must have a same-name Markdown guide. Each guide must state the scientific question, prerequisites, required forcing fields and units, complete configuration role, exact command, expected outputs, verification procedure and tolerances, limitations, appropriate interpretation, reproducibility metadata, and peer-reviewed references. Historical names such as `sar` do not activate object physics; guides must state whether the configuration is passive or explicitly selects a drift model.

Documentation tests must enforce the one-to-one configuration/guide relationship and the presence of required scholarly and reproducibility sections. Documentation-only changes must run those tests; physical or configuration changes must additionally run the portable core, serial application, relevant parallel/backend tests, restart tests, and forward/backward regression tests.
