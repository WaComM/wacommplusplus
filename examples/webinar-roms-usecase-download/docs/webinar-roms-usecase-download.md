# ROMS webinar forcing download and conversion

## Scientific question

Can the twenty-five hourly historical ROMS windows from 2026-09-15 00:00 through
2026-09-16 00:00 UTC be read and normalized into reusable native WaComM forcing? This is a
**dry preprocessing example**. It does not release particles or calculate drift,
concentration, decay, closure events, or search areas. The historical webinar
source is disabled. Conversion success is software verification, not
observational validation of the ROMS hindcast.

## Prerequisites

Run commands from the repository root. Build the application with NetCDF C++4
and a NetCDF C library supporting the remote protocol; inspect `nc-config --all`.
The runner additionally needs Python 3, `ncdump`, and a POSIX host (process-group
timeouts). This helper does not change portable-core build requirements.

```bash
cmake -S . -B build-webinar -DBUILD_APPLICATION=ON -DBUILD_TESTING=ON \
  -DUSE_MPI=OFF -DUSE_OMP=OFF -DUSE_CUDA=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build-webinar -j2
```

### Download the file archive

The supplied HTTP file archive is reachable independently of OPeNDAP. Download
the 25 configured files into a local mirror, preserving date directories:

```bash
python3 examples/webinar-roms-usecase-download/tools/download.py \
  --destination examples/webinar-roms-usecase-download/data/roms-20260915-16 \
  --workers 4
```

The default source is `https://data.meteo.uniparthenope.it/files/rms3/d03/history/`.
This window requires 155,758,245,200 bytes (about 156 GB). The downloader retains
partial transfers, checks NetCDF readability, records HTTP headers and physical
time metadata, and writes SHA-256 checksums under the mirror's `provenance/`.
Repeating the command verifies completed files against their recorded hashes
and resumes incomplete transfers. These locally computed hashes establish
archive identity; the provider supplies no independent checksum manifest.

Run `python3.11 examples/webinar-roms-usecase-download/tools/verify_download.py`
to reproduce the recorded size, time, grid, sigma-curve and angle checks using
NetCDF4 and NumPy. This metadata audit does not certify all field values or
supply an observational validation.

The [download record](download-20260915.json) confirms all requested files were
retrieved. The old OPeNDAP failures in [performance results](performance-results.md)
remain historical evidence. The application configuration still names OPeNDAP;
for local conversion, explicitly pass the mirror to the runner:

```bash
python3 examples/webinar-roms-usecase-download/tools/run.py \
  --binary build-webinar/wacommplusplus \
  --base-path examples/webinar-roms-usecase-download/data/roms-20260915-16-angle-repaired-001 \
  --run-root examples/webinar-roms-usecase-download/data/local-conversion-001 \
  --timeout 7200
```

**Explicit preprocessing:** run the [rectilinear angle repair](rectilinear-angle-repair.md)
before the local conversion command above. It checks the complete rho/U/V
geometry and preserves velocities and physical time exactly, while correcting
angle to zero under a declared ROMS grid-axis interpretation. The source
archive remains unchanged; original angle, policy and checksums are archived.
This resolves the preparation workflow under that assumption, without claiming
provider confirmation or correcting any upstream ROMS dynamical error. Native
smoke comparisons and the performance matrix remain separate required checks.

## Required forcing fields and units

These are the **required input conventions**, not a claim that the legacy ROMS
adapter validates every attribute. Inspect every file's metadata before reuse;
the adapter currently copies numeric time without epoch conversion and rotates grid-relative horizontal vectors according to the
[explicit angle/basis contract](../../../docs/adapters.md#roms-horizontal-vector-basis),
but does not perform generalized vertical-coordinate conversion. Reject a dataset whose conventions cannot be established.

| Fields | Layout and required meaning |
| --- | --- |
| `ocean_time` | Strictly increasing time, seconds since 1968-05-23 00:00:00 UTC, Gregorian calendar |
| `mask_rho`, `mask_u`, `mask_v` | Corresponding rho/U/V horizontal grids; dimensionless land/water masks, 0/1 |
| `lat_rho`, `lon_rho`, `lat_v`, `lon_u` | Corresponding horizontal grids; degrees north/east |
| `h`, `zeta` | Rho grid; depth and sea-surface elevation in m; `zeta` also has time |
| `s_rho`, `s_w` | Dimensionless bottom-to-surface levels, compatible with the existing sigma-coordinate model; not arbitrary stretched ROMS coordinates |
| `angle` | `(eta_rho,xi_rho)`, radians counterclockwise from east to XI; required for grid-relative U/V, with finite nonmissing values at wet rho points |
| `u`, `v` | `(ocean_time,s_rho,eta_u,xi_u)` and `(ocean_time,s_rho,eta_v,xi_v)`, `units="meter second-1"`; unlabeled ROMS components use `angle`, or both must carry the explicit eastward/northward velocity standard names to bypass rotation |
| `w`, `AKt` | `(ocean_time,s_w,eta_rho,xi_rho)`, vertical velocity m s-1 and diffusivity m2 s-1; case-sensitive `AKt` |

The adapter averages available wet staggered faces onto rho points; it does not
provide conservative flux regridding. See the [adapter description](../../../docs/adapters.md)
and its Shchepetkin and McWilliams (2005) reference for the ROMS context.

## Configuration

The paired [configuration](../webinar-roms-usecase-download.json) has these roles:

- `simulation`: identification, nominal UTC interval, and `dry=true`, which skips
  `Wacomm::run`. Dry conversion visits the configured input list; it is not a
  general time-subsetting downloader.
- `io`: explicit `ROMS` selection, HTTPS base plus twenty-five chronological relative
  paths, `save_input=true`, and native output prefix. `nc_output_root`,
  `mask_output`, and the 3600 s concentration-output timestep do not produce
  particle output in dry mode.
- `sources` and `restart`: both disabled, with inactive historical paths removed.
- `physics`: archived historical settings and seed 5489. Particle parameters and
  boundary closures are inactive during conversion; their units and definitions
  remain in the [configuration reference](../../../docs/configuration.md).
- `tracking.direction=forward`: visits windows chronologically. It does not negate
  physical velocities. This example does not test stochastic backward tracking
  or continuous-versus-restarted trajectories.

For a single conversion, first create a fresh destination (the serializer does
not create directories and replaces existing output files):

```bash
mkdir examples/webinar-roms-usecase-download/data/processed && \
OMP_NUM_THREADS=1 ./build-webinar/wacommplusplus \
  examples/webinar-roms-usecase-download/webinar-roms-usecase-download.json
```

The `mkdir` command deliberately fails if that directory already exists. Select
and record a fresh output prefix before repeating this direct command.

## Expected outputs

Expect twenty-five `ocm3_d03_20260915ZHH.nc` native forcing files, with `HH` from
00 through 23 plus `ocm3_d03_20260916Z00.nc`, under `data/processed/`. These contain normalized rho-grid U/V,
W, diffusivity, grid geometry, and physical time. They are converted datasets,
not byte-for-byte downloads of the remote ROMS source.

For one-record hourly inputs, each of the first twenty-four saved files also contains
the next window's boundary record; the final file contains only its own record.
Inspect actual input record counts before asserting this pattern for a different
archive. The application retains the current and adjacent normalized windows.
No particle or concentration output is expected.

Conceptual data flow (not a model result):

```text
ROMS window i + next boundary -> common ROMS adapter -> native forcing file i
                                             dry=true -> no particle solver
```

## Verification

```bash
ctest --test-dir build-webinar \
  -R 'webinar_roms_workflow|example_documentation|roms_adapter|native_adapter|particle_physical_interval|restart' \
  --output-on-failure
```

The workflow test generates twenty-five tiny synthetic ROMS files using `ncgen`,
runs the actual application for a warm-up and three repetitions, verifies saved
physical times and boundary records, checks exact repeatability, rejects archive
reuse, and verifies that invalid NetCDF produces a failed report without a
performance median. It is a regression fixture, not the historical dataset or
its performance result. Missing `ncgen`/`ncdump` skips this test explicitly.
The ROMS adapter test independently verifies staggered velocity mapping and
0, ±π/2, π and π/4 rotations within 1e-6 m s-1, tests metadata failures and
exact native round trips, and exercises forward/backward restart equivalence.

## Performance evaluation

Run the fixed download-and-conversion workload with a new archive directory:

```bash
python3 examples/webinar-roms-usecase-download/tools/run.py \
  --binary build-webinar/wacommplusplus \
  --run-root examples/webinar-roms-usecase-download/data/performance-001 \
  --timeout 600
```

The runner performs one warm-up and three measured repetitions, each in its own
output directory, with one directly launched process and `OMP_NUM_THREADS=1`.
The estimator is the median of three elapsed application wall times in seconds;
the minimum and maximum are descriptive ranges, not confidence intervals.
Timing includes startup, remote transport or local reads, normalization, native
writes, and shutdown; read-only `ncdump` comparisons and checksumming are excluded.
Warm-up may populate server and OS caches; these are not cold-download timings.
Shared-host contention is uncontrolled, so results are diagnostic only.

Every repetition must exit successfully, produce twenty-five readable files, and
match the warm-up's complete `ncdump -p 9,17` values and metadata exactly. File
checksums are archived separately because container bytes can differ. There is
no missing-value imputation or failed-sample exclusion: failure stops the suite,
records logs, and suppresses the median. This comparison establishes repeatability;
it does not establish correctness of unknown source metadata or observational skill.

Use `--base-path /absolute/path/to/roms-mirror` only for an explicitly staged mirror
preserving the configured `2026/09/15/...` and `2026/09/16/...` paths. Local input SHA-256 checksums are
recorded. Remote source hashes remain null: output hashes cannot authenticate
original source bytes. Preserve the source files and their provenance separately
for a reproducible offline comparison. Remote and local timings are different
workloads and must not be pooled.

The [shared solver scaling protocol](../../../docs/performance-evaluation.md)
requires a solver interval and particle-output equivalence. Both are absent here;
MPI/OpenMP/GPU solver speedup and efficiency are therefore **not applicable**.
Use a separately documented active simulation for that experiment. Review the
[performance evidence](performance-results.md) before interpreting any timings.

## Limitations

Historical archive availability, units, epoch, vector basis, and vertical-grid
compatibility remain prerequisites. No automatic data substitution, coordinate
transform, or unit conversion is introduced. Running multiple MPI ranks can
repeat adapter reads; it is not a supported download acceleration strategy here.
The example has no particle restart or forward/backward simulation result to
compare. Those scientific properties remain covered by the common model tests.

## Reproducibility

Keep immutable run roots under this example's ignored `data/` directory and
archive them externally for long-term use. `run.json` records revision, working
patch, runner and executable hashes, submitted configurations, forcing locations
and available hashes, seed, platform, CPU affinity, OpenMP settings, timeout
outcomes, raw samples, output hashes, and comparisons. Logs, CMake cache when
adjacent to the binary, and NetCDF configuration are retained. Archive the complete
build logs, compiler/dependency versions, executable, source files, and scheduler
allocation alongside these records; the runner does not package external data or
prove that an existing binary was built from the current checkout. Record the
absence of source/restart inputs and particle outputs explicitly.

## References

- Shchepetkin, A. F., and McWilliams, J. C. (2005). The regional oceanic modeling system (ROMS): a split-explicit, free-surface, topography-following-coordinate oceanic model. *Ocean Modelling*, 9, 347–404. [doi:10.1016/j.ocemod.2004.08.002](https://doi.org/10.1016/j.ocemod.2004.08.002).
- Hoefler, T., and Belli, R. (2015). Scientific benchmarking of parallel computing systems: twelve ways to tell the masses when reporting performance results. *Proceedings of the International Conference for High Performance Computing, Networking, Storage and Analysis*, article 73, 1–12. [doi:10.1145/2807591.2807644](https://doi.org/10.1145/2807591.2807644).
