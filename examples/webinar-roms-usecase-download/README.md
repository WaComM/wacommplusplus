# ROMS webinar download use case

This example downloads and converts twenty-five historical ROMS forcing windows in
**dry mode**. Sources and restart are disabled; no particles are advanced.

See the [configuration guide](docs/webinar-roms-usecase-download.md) for fields,
units, prerequisites, exact commands, verification, limitations, and reproducibility.
The [performance record](docs/performance-results.md) distinguishes conversion
wall time from the [shared solver scaling protocol](../../docs/performance-evaluation.md).

The September 2026 archive uses an [explicit angle-repair preprocessing policy](docs/rectilinear-angle-repair.md).
It preserves the original downloads and records the grid-axis interpretation
and complete geometry checks before native conversion. All 25 repaired files
were converted successfully; see the [executed record](docs/angle-repair-results.json)
and the [native performance workflow](../webinar-native-usecase/docs/performance-evaluation.md).

## References

- Hoefler, T., and Belli, R. (2015). Scientific benchmarking of parallel computing systems: twelve ways to tell the masses when reporting performance results. *SC '15*, article 73, 1–12. [doi:10.1145/2807591.2807644](https://doi.org/10.1145/2807591.2807644).
