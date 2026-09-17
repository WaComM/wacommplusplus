# Native webinar passive-release protocol

## Scientific question

How does MPI/OpenMP/CUDA execution cost change for the historical webinar's
seeded passive release during 15 September 2026 00:00 through 16 September 2026 00:00 UTC? The production
configuration releases 250,000 particles/hour. The Sarno problem-size protocol
additionally measures 1,000, 10,000, 100,000 and 1,000,000 particles/hour, each as
an independent fixed-workload suite. The historical name activates no object
physics. Numerical repeatability is verification, not observational validation.

## Prerequisites

Use Python 3.11 with the repository `tools/requirements-figures.txt` dependencies,
a Release serial application for preparation, and a single Release
MPI/OpenMP/CUDA binary in `build-cuda/` for every performance tuple. The supplied
Slurm launchers target the same 32-core GPU node class and module environment as
the Sarno protocol, including two matching nodes for 64 ranks and up to four
Tesla devices on one node. Check the module names and partition before use. Jobs request a 24-hour limit;
set `WEBINAR_TIME_LIMIT` explicitly if the partition requires another limit.
Run the portable, serial, parallel, forward/backward, stochastic and restart
checks before scheduling. Missing hardware leaves the matrix incomplete.

## Required forcing fields and units

Stage the 25 hourly files from `ocm3_d03_20260915Z00.nc` through
`ocm3_d03_20260916Z00.nc`, inclusive, from the declared native archive. The native adapter
requires grid longitude/latitude (degrees east/north), wet mask (dimensionless),
bathymetry (metres) and bottom-to-surface sigma levels (dimensionless), sea-surface height (metres),
eastward/northward/vertical velocity (m/s), vertical diffusivity (m²/s), and
explicit physical time metadata. Follow the exact native schema in the
[adapter reference](../../../docs/adapters.md). Do not assign an epoch or change
file order from tracking direction. Preparation requires the serializer's exact field names, dimensions, units,
Gregorian seconds since 1968-05-23 00:00:00 GMT, finite unmasked fields, valid
mask/geographic ranges and ascending sigma coordinates. It performs no unit,
epoch or calendar conversion. The legacy native reader does not independently
validate all metadata; do not bypass preparation. Source-grid geometry is
exercised by the shared application during the smoke run.
The corrected ROMS adapter rotates declared grid components before native
serialization; archived files identify eastward/northward components explicitly.
For this archive, the [explicit repair policy](../../webinar-roms-usecase-download/docs/rectilinear-angle-repair.md)
checks the complete rectilinear geometry and corrects angle in derived files
under a declared grid-axis interpretation. Original files remain unchanged.
The [preparation evidence](preparation-results.json) confirms native validation
and both serial smoke replays; it does not validate upstream ROMS dynamics.
The [ROMS conversion guide](../../webinar-roms-usecase-download/docs/webinar-roms-usecase-download.md)
describes the provider and normalization workflow. Its availability
is an external prerequisite; Sarno's 2021 forcing cannot replace this workload.

## Configuration

The [paired JSON](../webinar-native-usecase.json) retains the requested 24-hour
window, `WaComM` adapter, forward tracking, random transport and random source
placement, seed 5489, hourly gridded output, NetCDF history, and 7200 s restart
interval. Restart input is disabled. Its source is the declared point at
14.0468° E, 40.83480299863682° N, depth 0 m. Upper reflection, lower kill and
horizontal kill closures, decay and diffusion parameters remain those of the
paired configuration; see the [model](../../../docs/model.md) and
[configuration reference](../../../docs/configuration.md) for equations, units
and scope. No regridding, wind, wave or object model is added.

The launcher archives a copy of the source and changes only `emission.rate`
for each size; it preserves geometry and random placement. Every sample has an
isolated working directory that resolves the unchanged relative I/O paths.
Preparation uses 1,000 particles/hour twice as an untimed repeatability check.

## Expected outputs

Each run must produce 24 hourly gridded files and 12 two-hour particle histories.
It archives native gridded concentration under `output/`, particle history
under `restart/`, stdout/stderr, exit status and checksums. The history cadence
remains two hours; it is not changed to Sarno's snapshot cadence. The collector
compares every emitted history record and every gridded variable. Expected
solver boundaries are each adjacent hourly boundary in the requested day, exactly 24 intervals.

## Verification

The current archive is already prepared; do not overwrite it or rerun preparation
in place. To reproduce preparation in a fresh staging area, use the native
conversion output from the [repair workflow](../../webinar-roms-usecase-download/docs/rectilinear-angle-repair.md).
From the repository root, with the scientific Python environment:

```bash
data/wacomm-sarno-lite/venv/bin/python examples/webinar-native-usecase/tools/prepare_webinar.py \
  --forcing-dir examples/webinar-roms-usecase-download/data/native-conversion-angle-repaired-001/processed \
  --binary build-serial-rotation/wacommplusplus \
  --timeout 7200
```

Preparation copies and hashes the forcing and runs two serial replays. A failed
preparation stays archived and never opens the scheduling gate. Archive the
failed preparation before trying again. For a new, separately named rate suite (the current primary suite already has
a [scheduled continuation](performance-evaluation.md#scheduled-continuation-for-the-repaired-forcing)):

```bash
bash examples/webinar-native-usecase/tools/run_webinar_protocol.sh cpu webinar-q1000-001 low-gn 1000
python3.11 examples/webinar-native-usecase/tools/webinar_protocol_collect.py examples/webinar-native-usecase/data/webinar-q1000-001
bash examples/webinar-native-usecase/tools/run_webinar_protocol.sh gpu webinar-q1000-001 low-gn 1000
python3.11 examples/webinar-native-usecase/tools/webinar_protocol_collect.py examples/webinar-native-usecase/data/webinar-q1000-001
```

Wait for each scheduled phase to finish before collecting. Repeat with unique
suite names for 10,000, 100,000, 1,000,000 and the original 250,000 particles/hour.
An optional fifth launcher argument is a predecessor Slurm job ID. Resume only
after the previous jobs have finished or been cancelled, using
`resume_webinar_protocol_cpu.sh <suite-name> [partition]` or
`resume_webinar_protocol_gpu.sh <suite-name> [partition]` in the same tools folder.
Keep every failed attempt and actual scheduler order.

The [performance evaluation](performance-evaluation.md) specifies the full
matrix and estimators. CPU particle state and all gridded variables must agree
exactly by stable ID and physical time. GPU discrete/state/time values are
exact; horizontal great-circle separation is bounded by 10⁻⁶ m using radius
6,371,000 m, depth by 10⁻⁸ m, and dimensionless grid indices by 10⁻⁸. These are
inherited acceptance limits to test, not demonstrated webinar equivalence or
observational error bounds. Missing/masked values must have identical masks;
non-finite particle state fails. Incomplete or failed points never enter charts.

```bash
ctest --test-dir build-core --output-on-failure
ctest --test-dir build --output-on-failure
ctest --test-dir build-cuda --output-on-failure
python3.11 examples/webinar-native-usecase/tools/WebinarProtocolTest.py
```

Forward/backward and restart tests verify the shared solver; this forward-only
performance experiment is not itself a backward or restart-equivalence test.

## Limitations

Large September 2026 forcing and outputs are stored in ignored local archives,
not bundled in Git. Forcing preparation and serial smoke checks passed; the
[dated status record](performance-status.md) distinguishes submitted jobs from
validated performance results.
Do not publish a CPU selection, GPU recommendation or performance curve until
all required tuples pass. Population sampling density is not a physical flux;
changing it creates a different workload with its own baseline. No probability,
confidence region, unique backward origin or calibrated object drift is implied.

## Reproducibility

All example-specific run archives live under ignored `examples/webinar-native-usecase/data/`.
Keep complete configuration/source/forcing/executable/output checksums, revision
and working patch (including new tool files), random seed, CMake/compiler and
dependency versions, platform/topology, bindings, scheduler IDs, raw samples,
run order, comparison reports and plotting environment. Preserve the archive
externally before cleanup; no long-term external archive is currently published.
Version only validated compact tables, figures and captions under `docs/figures/`.

## References

- Amdahl, G. M. (1967). Validity of the single processor approach to achieving large scale computing capabilities. *AFIPS Spring Joint Computer Conference*, 30, 483–485. [doi:10.1145/1465482.1465560](https://doi.org/10.1145/1465482.1465560).
- Hoefler, T., and Belli, R. (2015). Scientific benchmarking of parallel computing systems: twelve ways to tell the masses when reporting performance results. *Proceedings of the International Conference for High Performance Computing, Networking, Storage and Analysis*, article 73, 1–12. [doi:10.1145/2807591.2807644](https://doi.org/10.1145/2807591.2807644).
- Montella, R., Di Luccio, D., De Vita, C. G., Mellone, G., Lapegna, M., Ortega, G., Marcellino, L., Zambianchi, E., and Giunta, G. (2023). A highly scalable high-performance Lagrangian transport and diffusion model for marine pollutants assessment. *31st Euromicro International Conference on Parallel, Distributed and Network-Based Processing*, 17–26. [doi:10.1109/PDP59025.2023.00012](https://doi.org/10.1109/PDP59025.2023.00012).
