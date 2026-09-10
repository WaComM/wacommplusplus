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

No accelerator or backend may contain a separate physical model. ROMS, NEMO, HYCOM, native WaComM, WRF, WW3, and future ocean, weather, or wave adapters normalize physical data only. They must not negate velocities, reverse physical time, reverse file order, or select diffusion or environmental forcing from tracking direction. Coordinate rotation into documented Earth-relative components is normalization, not direction policy. Direction belongs to the solver.

Environmental adapters must validate variable identity, dimensions, units, calendar, time origin, coordinate orientation, missing values, and finite ranges before exposing a field to physics. Unit or calendar conversion must be explicit, mathematically documented, and regression-tested; ambiguous or unsupported metadata must fail rather than inherit an implicit scale or epoch. New CF aliases require a fixture using the alias and its declared units. Regridding and projection transforms are separate numerical operators and must not be hidden inside a product-name alias.

Regridding is opt-in configuration and must name the numerical operator. Every regridding implementation must document its coordinate reference system, stencil, weights, vector basis, domain-of-validity, extrapolation policy, conservation properties, error behavior, and unsupported grids. Tests must cover constants, analytically representable fields, boundaries, invalid geometry, and forward/backward use. A non-conservative interpolant must never be described as conservative, and a projection transform must never be inferred from coordinate values alone.

Spatial acceleration structures may reduce candidate discovery but must not change the interpolation operator, tolerance, source-order tie breaking, geometry validation, extrapolation behavior, or vector basis. Index construction and query assumptions, asymptotic limitations, cyclic-coordinate treatment, and equivalence tests belong in the same change.

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

Uncertainty models are part of the governing scientific configuration. Document the sampled quantity, distribution, units, dependence assumptions, support or truncation, keying strategy, temporal correlation, restart behavior, and interpretation. A parameter ensemble must be reproducible from archived metadata and invariant under rank, thread, accelerator scheduling, and restart decomposition. Do not label an ensemble as probabilistic calibration, confidence, or posterior inference without peer-reviewed methodology and observational validation supporting that claim.

WaComM++ is one versioned scientific product comprising physics, implementation, tests, examples, and documentation. Review releases across all five layers.

Documentation is a scientific interface and must remain semantically synchronized with the implementation. A change to equations, parameter values, units, sign conventions, configuration keys, defaults, environmental inputs, object catalogs, restart variables, output variables, adapters, execution backends, or validation tolerances must update the model description, configuration reference, relevant example guides, main README, and repository reference list in the same change.

Scientific narrative must meet the standard of a peer-reviewed computational modeling article. Distinguish governing equations, empirical parameterizations, numerical methods, software architecture, verification evidence, scientific validation evidence, assumptions, uncertainty, limitations, and interpretation. Never present numerical agreement as observational validation, a stochastic backward ensemble as a unique inverse trajectory, or a catalog coefficient as universally valid outside its documented experimental scope.

Every document that states model physics or describes scientific/runtime configuration must contain an explicit `References` section. Normative scientific claims must cite verified peer-reviewed journal articles or peer-reviewed conference proceedings with persistent identifiers where available. Technical reports, manuals, websites, source-code comments, and third-party data catalogs may document provenance or interoperability, but they must not replace peer-reviewed support for a physical or numerical claim. Verify author list, title, venue, year, volume/pages, and DOI before adding a citation.

Every checked-in example configuration must have a same-name Markdown guide. Each guide must state the scientific question, prerequisites, required forcing fields and units, complete configuration role, exact command, expected outputs, verification procedure and tolerances, limitations, appropriate interpretation, reproducibility metadata, and peer-reviewed references. Historical names such as `sar` do not activate object physics; guides must state whether the configuration is passive or explicitly selects a drift model.

Documentation tests must enforce the one-to-one configuration/guide relationship and the presence of required scholarly and reproducibility sections. Documentation-only changes must run those tests; physical or configuration changes must additionally run the portable core, serial application, relevant parallel/backend tests, restart tests, and forward/backward regression tests.

Documentation must use figures, schemas, equations, and maps when they materially clarify architecture, physical-vector composition, grid relationships, temporal logic, ensemble interpretation, or spatial behavior. Visual assets must be repository-native, versioned, accessible, legible in light and dark reading contexts, and accompanied by descriptive alternative text and a caption or adjacent narrative stating whether the visual is conceptual, diagnostic, or a model result. Scientific maps must identify projection, spatial extent, time, units, data/coastline provenance, and uncertainty semantics; conceptual maps must be explicitly labeled non-georeferenced. Equations must define every symbol and unit. Never use decorative graphics as evidence, present a schematic as output, or allow a visual to contradict code, configuration, tests, or the documented mathematical model.

Diagnostic tooling must be read-only with respect to simulation output and must preserve stable trajectory identity and physical time. Every statistic must define its estimator, units, selection rules, missing-data treatment, coordinate geometry, and inferential limits. Maps must not download or fabricate geographic context implicitly, and ensemble paths or envelopes must not be labeled as probabilities, confidence regions, or search areas unless the implementation and validation support that interpretation.
