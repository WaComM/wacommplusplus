# Webinar data staging

Large forcing, simulation outputs and execution archives are excluded from Git.
The [configuration guide](../docs/webinar-native-usecase.md) defines the scientific
workload and provenance requirements; the [dated status](../docs/performance-status.md)
distinguishes completed checks from pending performance results.

| Local directory | Contents |
| --- | --- |
| `processed/` | 25 validated native forcing files for September 15–16, 2026; reused read-only |
| `preparation/` | Archived serial executable, two 1,000-particles/hour smoke replays and exact comparison evidence |
| `preparation/provenance/` | Forcing manifest, build/configuration provenance, repair record and `smoke-passed.txt` scheduling gate |
| `webinar-q250000-angle-repaired-001/` | Primary CPU/GPU suite, per-tuple samples, raw logs and scheduler evidence |
| `webinar-q250000-angle-repaired-001/provenance/continuation/` | Archived continuation script, tool checksums, requested protocol and schedule |
| `webinar-q250000-angle-repaired-001/publication/` | Created only after complete CPU/GPU validation; compact results and charts |

The original downloads, repaired ROMS subset and native conversion archive live
under `examples/webinar-roms-usecase-download/data/`. Their checksums and locations
are recorded in the [repair results](../../webinar-roms-usecase-download/docs/angle-repair-results.json).
Do not overwrite preparation or existing suites. Preserve archives externally
before cleanup; no external long-term archive has been published.

## References

See the [guide references](../docs/webinar-native-usecase.md#references).
