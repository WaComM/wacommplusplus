# Native-WACOMM webinar use case

Seeded passive transport for **2026-09-15 00:00–2026-09-16 00:00 UTC**, using
24 two-record hourly native forcing files. The twenty-fifth converted file is
the already represented terminal boundary and remains preparation provenance.
The primary performance workload releases
250,000 particles/hour with seed 5489.

- [Configuration guide](docs/webinar-native-usecase.md): fields, configuration, commands, verification and limitations.
- [Performance protocol](docs/performance-evaluation.md): MPI, OpenMP, hybrid and selected-CPU GPU sweeps, estimators and scheduled continuation.
- [Execution status](docs/performance-status.md): dated preparation evidence, submitted jobs and outstanding validation.
- [Data archives](data/README.md): locations of forcing, smoke checks and performance artifacts.

Forcing conversion and two reduced-count serial smoke replays passed. The primary
CPU matrix and conditional GPU/final-collection workflow are scheduled; this does
not yet constitute a validated performance result. Additional release rates are
separate problem-size experiments with their own baselines.

The September 2026 inputs use an [explicit angle-repair policy](../webinar-roms-usecase-download/docs/rectilinear-angle-repair.md).
It preserves original downloads and records the grid-axis interpretation and
complete geometry checks before native conversion. This assumption does not
certify upstream ROMS dynamics or observational accuracy.

## References

See the peer-reviewed references in the [configuration guide](docs/webinar-native-usecase.md#references).
