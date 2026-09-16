# Historical ROMS conversion performance record

## File-archive recovery

The user-supplied `/files/rms3/d03/history/` endpoint successfully delivered all
25 requested files (155,758,245,200 bytes). See the [download and metadata record](download-20260915.json).
The earlier OPeNDAP timeouts below remain archived. Download durations are not
solver timings, and no conversion median or scientific benchmark follows from
transfer success. An [explicit angle-repair policy](rectilinear-angle-repair.md)
now provides derived forcing under a documented grid-axis interpretation.
The [executed record](angle-repair-results.json) contains all 25 repaired and
25 converted native file checksums. Serial conversion exited successfully in
406.171 seconds. This single preparation duration is not a protocol median;
repair and conversion are not solver timing samples.

## Updated 2026-09-15/16 window

The current configuration requests 25 hourly inputs from 2026-09-15 00:00 through
2026-09-16 00:00 UTC. A network-enabled HTTPS metadata probe for the first file
returned no bytes within 30 seconds (curl exit 28). The updated conversion
warm-up also timed out after 30.077 s, with no outputs or eligible samples.
Its [separate run record](performance-attempt-20260915.json) preserves the full
requested file list, binary/configuration hashes and failed sample. This attempt
used the existing Release MPI/OpenMP application with one directly launched
process and one thread; CUDA was disabled. It does not supersede or reuse the
2019 measurements below.

```bash
python3 examples/webinar-roms-usecase-download/tools/run.py \
  --binary build/wacommplusplus \
  --run-root examples/webinar-roms-usecase-download/data/performance-20260915-24h-001 \
  --timeout 30
```

The synthetic 25-file conversion regression passes for the midnight boundary.
At that earlier attempt, provider data were unavailable and no valid result
was produced. The later file-archive recovery, repair and single conversion
above supersede that readiness state. Native smoke checks now pass, as recorded
in the [native example status](../../webinar-native-usecase/docs/performance-status.md);
no repeated-conversion median or validated solver performance curve is implied.

## Scope and estimator

This record concerns the thirteen-file 2019-04-01 dry conversion used by the previous configuration (the
[current guide](webinar-roms-usecase-download.md) now describes the September 2026 window). Application wall seconds
include startup, transport, normalization, output, and shutdown. A valid suite
requires one successful warm-up and three successful, exactly repeatable measured
runs. The estimator is their median; failed elapsed time is not throughput.

## Endpoint diagnosis

On 2026-09-15, HTTP returned status 302 with this malformed `Location`:

```text
https://data.meteo.uniparthenope.itopendap/rms3/d03/history/2019/04/01/rms3_d03_20190401Z0800.nc.dds
```

The direct HTTPS metadata request timed out after 30 seconds without receiving
bytes. These checks used network-enabled execution. The checked-in URL now uses
HTTPS to avoid the known broken redirect; that change cannot repair availability
of the external server.

## Results and interpretation

The network-enabled serial application attempt is archived in
[`performance-attempt.json`](performance-attempt.json). It used the parent revision
`2a1183b9c74b0b21c4b084371a844fa2b29cab3f` plus the example changes in this commit, a rebuilt Release
binary with MPI/OpenMP/CUDA disabled, and a 30 s per-process deadline.
The exact invocation was:

```bash
python3 examples/webinar-roms-usecase-download/tools/run.py \
  --binary build/wacommplusplus \
  --run-root examples/webinar-roms-usecase-download/data/performance-20260915-https-03 \
  --timeout 30
```

Use a new archive path for any repeat.

| Stage | Outcome | Elapsed wall seconds | Eligible samples |
| --- | --- | ---: | ---: |
| Historical HTTPS warm-up | Timed out; process group terminated | 30.003679 | 0 |
| Three measured repetitions | Not started after failed warm-up | — | 0 |
| Solver CPU/MPI/OpenMP/GPU sweeps | Not applicable: dry mode skips solver | — | 0 |

There is no median, throughput, speedup, efficiency, CPU selection, or GPU
recommendation. No scientific output comparison completed for the historical
forcing. A chart would misleadingly present a failed duration as performance,
so this failure table is the diagnostic result.

The refreshed portable-core suite passed all 16 tests. The rebuilt MPI/OpenMP
suite passed all 31 tests, including the workflow and MPI/OpenMP parity checks. The serial application
suite passed its 27 existing tests; the new end-to-end workflow test also passed
following runner fixes. Its synthetic forcing is used only for regression.
The test covers exact repeatability, expected boundary times, invalid input,
archive reuse, and timeout handling. These checks do not validate the unavailable
historical forcing.

## Review and reproducibility

The local immutable archive is
`examples/webinar-roms-usecase-download/data/performance-20260915-https-03/` (ignored by Git); it contains the command, complete submitted
configuration, CMake cache, build/configuration logs, archived executable and checksum, raw
stdout/stderr, timeout record, and working patch. Source files were unavailable,
so their checksums are null. No external long-term archive has been published.
The compact record above preserves the failure evidence in Git. Earlier setup
attempts `performance-20260915-https` and `performance-20260915-https-02` failed
before application startup on Python 3.6 compatibility and missing optional
`nc-config`, respectively; those runner issues were fixed and are not timing
samples. Bibliographic metadata were checked against Crossref on 2026-09-15.

Inspect the archived stderr and run record before rerunning. Restore access to
the same forcing archive or explicitly stage a checksummed mirror with verified
metadata. Use a new run root and the guide's exact command. Do not substitute
synthetic fixture timings for this workload, compute speedup from a failed run,
or change the physical model to work around a transport failure.

## References

- Hoefler, T., and Belli, R. (2015). Scientific benchmarking of parallel computing systems: twelve ways to tell the masses when reporting performance results. *SC '15*, article 73, 1–12. [doi:10.1145/2807591.2807644](https://doi.org/10.1145/2807591.2807644).
