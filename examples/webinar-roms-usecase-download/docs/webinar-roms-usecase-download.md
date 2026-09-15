# ROMS webinar forcing download and conversion

## Scientific question

Can the thirteen hourly historical ROMS windows from 2019-04-01 08:00 through
20:00 UTC be read and normalized into reusable native WaComM forcing? This is a
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

Check the exact endpoint before a large transfer:

```bash
curl --fail --show-error --location --max-time 30 \
  'https://data.meteo.uniparthenope.it/opendap/rms3/d03/history/2019/04/01/rms3_d03_20190401Z0800.nc.dds'
```

On 2026-09-15 the old HTTP URL returned a malformed redirect with the slash
missing between the hostname and `opendap`. The configuration now requests HTTPS
directly. HTTPS availability is not guaranteed: see the [recorded attempt](performance-results.md).
Do not disable certificate verification or substitute another dataset silently.

## Required forcing fields and units

These are the **required input conventions**, not a claim that the legacy ROMS
adapter validates every attribute. Inspect every file's metadata before reuse;
the adapter currently copies numeric time without epoch conversion and does not
perform arbitrary ROMS grid-vector rotation or generalized vertical-coordinate
conversion. Reject a dataset whose conventions cannot be established.

| Fields | Layout and required meaning |
| --- | --- |
| `ocean_time` | Strictly increasing time, seconds since 1968-05-23 00:00:00 UTC, Gregorian calendar |
| `mask_rho`, `mask_u`, `mask_v` | Corresponding rho/U/V horizontal grids; dimensionless land/water masks, 0/1 |
| `lat_rho`, `lon_rho`, `lat_v`, `lon_u` | Corresponding horizontal grids; degrees north/east |
| `h`, `zeta` | Rho grid; depth and sea-surface elevation in m; `zeta` also has time |
| `s_rho`, `s_w` | Dimensionless bottom-to-surface levels, compatible with the existing sigma-coordinate model; not arbitrary stretched ROMS coordinates |
| `u`, `v` | `(ocean_time,s_rho,eta_u,xi_u)` and `(ocean_time,s_rho,eta_v,xi_v)`, m s-1; components must already have the basis supported by the adapter |
| `w`, `AKt` | `(ocean_time,s_w,eta_rho,xi_rho)`, vertical velocity m s-1 and diffusivity m2 s-1; case-sensitive `AKt` |

The adapter averages available wet staggered faces onto rho points; it does not
provide conservative flux regridding. See the [adapter description](../../../docs/adapters.md)
and its Shchepetkin and McWilliams (2005) reference for the ROMS context.

## Configuration

The paired [configuration](../webinar-roms-usecase-download.json) has these roles:

- `simulation`: identification, nominal UTC interval, and `dry=true`, which skips
  `Wacomm::run`. Dry conversion visits the configured input list; it is not a
  general time-subsetting downloader.
- `io`: explicit `ROMS` selection, HTTPS base plus thirteen chronological relative
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

Expect thirteen `ocm3_d03_20190401ZHH.nc` native forcing files, with `HH` from
08 through 20, under `data/processed/`. These contain normalized rho-grid U/V,
W, diffusivity, grid geometry, and physical time. They are converted datasets,
not byte-for-byte downloads of the remote ROMS source.

For one-record hourly inputs, each of the first twelve saved files also contains
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

The workflow test generates thirteen tiny synthetic ROMS files using `ncgen`,
runs the actual application for a warm-up and three repetitions, verifies saved
physical times and boundary records, checks exact repeatability, rejects archive
reuse, and verifies that invalid NetCDF produces a failed report without a
performance median. It is a regression fixture, not the historical dataset or
its performance result. Missing `ncgen`/`ncdump` skips this test explicitly.
The ROMS adapter test independently verifies staggered velocity mapping within
1e-6 m s-1 and exercises forward/backward restart equivalence.

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

Every repetition must exit successfully, produce thirteen readable files, and
match the warm-up's complete `ncdump -p 9,17` values and metadata exactly. File
checksums are archived separately because container bytes can differ. There is
no missing-value imputation or failed-sample exclusion: failure stops the suite,
records logs, and suppresses the median. This comparison establishes repeatability;
it does not establish correctness of unknown source metadata or observational skill.

Use `--base-path /absolute/path/to/roms-mirror` only for an explicitly staged mirror
preserving the configured `2019/04/01/...` paths. Local input SHA-256 checksums are
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
