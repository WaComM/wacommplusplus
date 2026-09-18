# Webinar native performance evaluation

## Scientific question and workload

Apply the full [shared protocol](../../../docs/performance-evaluation.md) and
[Sarno procedure](../../wacomm-sarno-lite/docs/performance-evaluation.md) to the
[webinar configuration](webinar-native-usecase.md). Each rate has the same
2026-09-15 00:00–2026-09-16 00:00 UTC forcing, seed 5489, random source placement and
output cadence. Measure the four Sarno rates plus the original 250,000/hour.
Never pool rates or copy Sarno timings, physical intervals or conclusions.

## Applying the requested protocol

The [requested development protocol](https://raw.githubusercontent.com/WaComM/wacommplusplus/refs/heads/development/docs/performance-evaluation.md)
was fetched again on 2026-09-16 and matches the repository copy byte-for-byte
(SHA-256 `18f28ab280f97662939fb67d05a0ed98fecc58f4c9037b91d0353b206da5c484`).
The primary fixed-workload experiment uses the original **250,000 particles/hour**
and the requested 24-hour window. The additional Sarno rates remain separate
problem-size experiments, each requiring its own complete matrix and baseline.

After the guide's forcing preparation succeeds, execute the following stages in
order, waiting for each scheduled phase to finish before collection:

```bash
bash examples/webinar-native-usecase/tools/run_webinar_protocol.sh cpu webinar-q250000-001 low-gn 250000
python3.11 examples/webinar-native-usecase/tools/webinar_protocol_collect.py examples/webinar-native-usecase/data/webinar-q250000-001
bash examples/webinar-native-usecase/tools/run_webinar_protocol.sh gpu webinar-q250000-001 low-gn 250000
python3.11 examples/webinar-native-usecase/tools/webinar_protocol_collect.py examples/webinar-native-usecase/data/webinar-q250000-001
```

Each suite archives the protocol and its checksum. GPU submission reruns the
CPU evidence checks and stages the archived CPU executable, preserving build
identity even if the workspace binary has since changed. Set `WEBINAR_PYTHON`
if the Python 3.11 scientific environment uses another executable path.
Review each generated `codex-performance-review.md` with its raw artifacts;
failed or incomplete points cannot support speedup or resource recommendations.
The dry ROMS conversion has no particle solver: its application-wall benchmark
prepares forcing and cannot supply the solver timings required by this protocol.

## Scheduled continuation for the repaired forcing

The primary suite is `data/webinar-q250000-angle-repaired-001`. CPU jobs
6712–6759 implement the matrix below. [Continuation job 6760](protocol-continuation.json)
depends on successful completion of job 6759. The archived
`tools/webinar_protocol_finish.sh` then invokes the GPU launcher, whose first
step independently validates the complete CPU matrix and selects the one-node
CPU reference. It schedules final collection after the last GPU job, and exports
the complete result to the suite's `publication/` directory. No GPU tuple is
selected from incomplete timings. No chart is accepted from a failed comparison.

The continuation verifies SHA-256 identities of its launcher, job runner,
collectors and comparison helpers before using the working tree. Changes cause
it to stop for review. It records scheduler settings and Python package versions;
its output/error logs and final collector job ID remain in the suite archive.
The original CPU executable is reused for GPU execution. Validation jobs run on
`low-wn`; measured jobs run exclusively on `low-gn`, which supports the declared
24-hour limit. The one-hour and twelve-hour partitions require shorter explicit
`WEBINAR_TIME_LIMIT` settings. Additional workload rates remain separate suites.

This scheduled chain is execution infrastructure, not a completed performance
result. Review `collect-cpu.err`, `collect-final.err`, the per-tuple review notes
and publication artifacts after completion. Failed dependencies do not authorize
skipping tuples or selecting an alternative workload.

The angle-repaired diagnostic chain was cancelled at jobs 7015--7058 after
all intervals in job 7015 completed but OpenMPI remained blocked in UCX during
`MPI_Finalize`. The installed OpenMPI 4.1.4 module reports UCX 1.15 while its
PML warns that UCX 1.18 or newer is required. The replacement suite explicitly
records and exports `OMPI_MCA_pml=ob1` and
`OMPI_MCA_btl=self,vader,tcp` for every sample. Results from the earlier UCX
suite are excluded rather than combined with the replacement because MPI
transport is part of the measured runtime environment.

## Matrix and execution

| Sweep | MPI ranks / OpenMP threads / devices |
| --- | --- |
| MPI | 1, 2, 4, 8, 16, 32, 64 / 1 / 0 |
| OpenMP | 1 / 1, 2, 4, 8, 16, 32 / 0 |
| Fixed 32 workers | 1/32/0, 2/16/0, 4/8/0, 8/4/0, 16/2/0, 32/1/0 |
| GPU | Selected one-node CPU placement / 0, 1, 2, 3, 4 devices |

Measure each of the 16 unique CPU tuples once per block, after one warm-up,
with three independent rotated replicate blocks. Select the smallest median
CPU solver time across all tuples, tie-breaking by ranks then threads. Select
the fastest one-node CPU tuple separately if needed for GPU execution. Keep
that placement fixed for the four GPU points. CPU masking, rank environment
export, core binding, GPU topology and telemetry follow the Sarno job runner.
See the [guide](webinar-native-usecase.md#verification) for exact commands.

The first `32/1/0` warm-up on job 6717 was killed by the node out-of-memory
handler after 16-rank execution had succeeded. A two-node retry in job 6785
completed all 24 solver intervals but did not terminate after the final forcing
boundary and was cancelled without contributing a sample. Both immutable
failed attempts are retained. Resumed CPU jobs therefore cap placement at
eight MPI ranks per homogeneous 178 GB Xeon Gold 5218 node: `32/1/0` uses four
nodes and `64/1/0` uses eight. Nodes with and without V100 devices have the same
recorded CPU model, core count, NUMA count and RAM; CUDA remains masked for all
CPU runs. The worker-count rule still applies, so other requested tuples retain
their minimum 32-core-node placement unless rank memory requires more nodes.
This changes resource placement, not the workload or equations. The overall
CPU optimum may consequently be a multi-node result; GPU comparison uses the
fastest validated one-node V100 tuple.

Live stack traces from the four-node retry showed ranks blocked in HDF5 reads
inside `WacommAdapter::process()` while handling the final native file. The
workload staging audit therefore checks every native time axis before deciding
whether a terminal artifact is redundant. The common driver is not shortened:
a single file may itself contain a valid interval, and the application
regression suite must continue to execute that case. Any replacement suite
must retain identical physical interval boundaries and output times.

```mermaid
flowchart LR
    A[Verify September 2026 native forcing] --> B[Two serial smoke replays]
    B --> C[16 CPU tuples × 3 blocks]
    C --> D[Compare outputs and select CPU references]
    D --> E[Four GPU counts × 3 blocks]
    E --> F[Compare outputs and publish complete suite]
```

Conceptual workflow, not simulation output. Repeat the entire chain after
preparation for each declared rate; preparation is reused read-only.

## Estimators, verification and interpretation

For rate q (particles/hour), ranks p, threads n and devices g (counts), let
T(q,p,n,g) be the median of three sums of the 24 positive finite solver
interval durations, in seconds. CPU speedup is T(q,1,1,0)/T(q,p,n,0), and CPU
efficiency is speedup/(p n); both are dimensionless. GPU incremental speedup
uses the selected one-node CPU time divided by GPU time. GPU device speedup
uses the one-device time divided by g-device time at fixed CPU placement;
device efficiency divides that ratio by g. The zero-device point has no device
efficiency. Application elapsed seconds include I/O but exclude preparation
and scheduler waiting, and are reported separately. No failed duration is a
sample. Three repetitions provide observed ranges, not confidence intervals.

The collector checks exact interval coverage, resource counts, warm-up success,
forcing hashes, sample identity and all saved particle/gridded output against
the one-worker reference. Numerical limits are stated in the guide. Existing
validation records are recomputed on collection. Preserve incomplete tuples
with their Codex review note; full collection must pass before selection.

## Publication and reproducibility

After CPU and GPU validation, export each rate with:

```bash
python3.11 tools/performance_publish.py examples/webinar-native-usecase/data/webinar-q1000-001 examples/webinar-native-usecase/docs/figures/performance-q1000 --physical-window '2026-09-15T00:00:00Z/2026-09-16T00:00:00Z'
```

Record excluded job IDs with `--excluded-job`. After all four protocol rates
complete, run:

```bash
python3.11 tools/performance_workload_scaling.py \
  --case 1000:examples/webinar-native-usecase/data/webinar-q1000-001 \
  --case 10000:examples/webinar-native-usecase/data/webinar-q10000-001 \
  --case 100000:examples/webinar-native-usecase/data/webinar-q100000-001 \
  --case 1000000:examples/webinar-native-usecase/data/webinar-q1000000-001 \
  --output-dir examples/webinar-native-usecase/data/workload-evaluation-001
```

Publish CPU speedup/efficiency, GPU incremental/device scaling, compact raw
samples and per-tuple review notes. Add descriptive alternative text and captions
identifying computational diagnostics, window, rate, hardware and sample count.
Archive dependency versions and figure hashes. See [status](performance-status.md)
for current coverage; no synthetic or Sarno result fills a missing webinar point.

## References

- Amdahl, G. M. (1967). Validity of the single processor approach to achieving large scale computing capabilities. *AFIPS Spring Joint Computer Conference*, 30, 483–485. [doi:10.1145/1465482.1465560](https://doi.org/10.1145/1465482.1465560).
- Hoefler, T., and Belli, R. (2015). Scientific benchmarking of parallel computing systems: twelve ways to tell the masses when reporting performance results. *Proceedings of the International Conference for High Performance Computing, Networking, Storage and Analysis*, article 73, 1–12. [doi:10.1145/2807591.2807644](https://doi.org/10.1145/2807591.2807644).
- Montella, R., Di Luccio, D., De Vita, C. G., Mellone, G., Lapegna, M., Ortega, G., Marcellino, L., Zambianchi, E., and Giunta, G. (2023). A highly scalable high-performance Lagrangian transport and diffusion model for marine pollutants assessment. *31st Euromicro International Conference on Parallel, Distributed and Network-Based Processing*, 17–26. [doi:10.1109/PDP59025.2023.00012](https://doi.org/10.1109/PDP59025.2023.00012).
