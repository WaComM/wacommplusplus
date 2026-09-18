# Webinar protocol execution status

## Recorded status on 2026-09-17

Forcing preparation and both serial smoke replays are complete. The primary
250,000-particles/hour CPU matrix was submitted as jobs **6712–6759** on
`low-gn`. Jobs 6712–6716 completed their first measured samples, but the
`32/1/0` warm-up in job **6717** was killed by the node out-of-memory handler.
Jobs 6718–6760 were consequently held by failed dependencies. The failed
attempt and its binding and runtime logs are retained. Resumed execution places
at most eight MPI ranks on each homogeneous 178 GB Xeon Gold 5218 node while
preserving the workload. The intervening two-node retry, job 6785, completed
all solver intervals but did not terminate at the final forcing boundary and
was cancelled without yielding a sample. See the performance protocol for the
resulting four- and eight-node placements. This is a dated execution record,
not a completed performance result.

| Stage | Recorded evidence | Remaining work |
| --- | --- | --- |
| ROMS download | 25 original files; checksums and physical times verified | None |
| Explicit angle repair | 25 derived files; complete grid checks passed | None under the documented interpretation |
| Native conversion | 25 files converted and validated; 24 two-record files are solver inputs and the terminal-boundary file is provenance-only | None |
| Serial smoke replays | Two successful runs; exact particle and gridded agreement | None |
| Primary CPU matrix | First samples through `16/1/0` completed; `32/1/0` jobs 6717 and 6785 failed before producing an eligible sample | Resume with the documented eight-ranks-per-node memory placement and validate every tuple |
| Selected-CPU GPU matrix | Conditional dispatch scheduled through job 6760 | Select CPU reference after validation; run four device counts |
| Final results | Conditional collection and export configured | Validate complete CPU/GPU sweep before accepting charts |
| Additional release rates | Separate suites documented | Not yet submitted |

## Forcing and preparation evidence

The original archive contains 155,758,245,200 bytes covering **2026-09-15
00:00–2026-09-16 00:00 UTC**, including all 25 hourly boundaries. The supplied
`/files/rms3/d03/history/` endpoint succeeded after earlier OPeNDAP timeouts.
See the [download record](../../webinar-roms-usecase-download/docs/download-20260915.json).

The [explicit repair policy](../../webinar-roms-usecase-download/docs/rectilinear-angle-repair.md)
assumes ROMS grid-axis components, checks the entire east/north rectilinear
C-grid, and sets angle to zero in derived copies. The original downloads and
source angle are preserved. All geometry residuals were zero, and retained
fields other than angle were unchanged. This is a documented preprocessing
interpretation, not provider confirmation or validation of upstream dynamics.

The [repair and conversion record](../../webinar-roms-usecase-download/docs/angle-repair-results.json)
contains checksums for 25 derived files (56,388,777,875 bytes) and 25 native
files (95,568,478,517 bytes). Serial conversion exited successfully in
406.171 seconds; this single preparation duration is not a solver benchmark.

The [smoke evidence](preparation-results.json) records two seeded serial runs at
1,000 particles/hour. Each produced 24 hourly gridded outputs and 12 two-hour
particle histories. Nonempty particle histories and all compared gridded fields
agreed exactly. This demonstrates repeatability for the reduced workload; it
does not establish MPI/OpenMP/GPU equivalence at the primary workload. Live
stack evidence from jobs 6785 and 6871 showed prolonged HDF5 reads while every
rank decoded the one-record terminal artifact after all 24 intervals. The
staged solver configuration now uses the 24 two-record files that cover those
same intervals and preserves the terminal file in conversion provenance.

## Scheduled protocol and result locations

The [CPU submission record](cpu-submission-angle-repaired.json) and
[continuation record](protocol-continuation.json) describe the active suite:

`examples/webinar-native-usecase/data/webinar-q250000-angle-repaired-001/`

After CPU jobs succeed, job 6760 independently validates their evidence and
selects the fastest GPU-compatible CPU tuple. Only then does it submit the
1-, 2-, 3- and 4-device runs, with warm-ups and three rotated replicate blocks.
Final collection depends on successful completion of the GPU jobs. Tool checksum
changes or failed comparisons stop progression. See the
[scheduled workflow](performance-evaluation.md#scheduled-continuation-for-the-repaired-forcing).

Inspect `collect-cpu.out`, `collect-cpu.err`, `collect-final.out`,
`collect-final.err`, and per-tuple `codex-performance-review.md` files in that
suite. A successful final export creates `publication/`, containing compact
results and checksummed charts. The final collector job ID is recorded in
`provenance/final-collector-job.txt` after GPU submission.

No final speedup, CPU optimum or GPU recommendation has been accepted. The
1,000, 10,000, 100,000 and 1,000,000 particles/hour problem-size suites remain
separate experiments, each requiring its own baseline and complete matrix.

## Implementation verification

The [rotation verification record](rotation-validation.json) archives 17
portable-core, 28 serial-application and 32 MPI/OpenMP-application passes, plus
eight GPU-node passes in job 6696 with no skips. Coverage includes analytic
rotations, metadata rejection, native round trips, and deterministic/seeded
forward/backward restart checks. The subsequent angle-repair application test
also passed; its evidence is in the repair record. The webinar protocol test
checks the 25-file window and all 24 solver intervals, including rejection of
incorrect time units and incomplete coverage. These tests support implementation
verification, not ocean-model observational skill.

## Historical failures and reproducibility

Earlier conversion attempts timed out before producing usable forcing, and the
launcher correctly rejected the missing preparation gate. Those failures are
preserved in the [historical conversion record](../../webinar-roms-usecase-download/docs/performance-results.md)
and its linked attempt files. They no longer describe current forcing readiness.

Keep immutable suite names and preserve every failed attempt with its raw logs
and review note. Archive configurations, forcing/source/restart/output hashes,
seed, executable, build options, working changes, dependencies, topology,
bindings, scheduler records and comparison tolerances. Follow the
[configuration guide](webinar-native-usecase.md#reproducibility). Do not change
physics, source geometry, time window or output cadence to improve timings.

## References

See the peer-reviewed [protocol references](performance-evaluation.md#references)
and the [repair references](../../webinar-roms-usecase-download/docs/rectilinear-angle-repair.md#references).
