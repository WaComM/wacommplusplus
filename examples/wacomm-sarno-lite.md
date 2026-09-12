# Sarno River: six-hour coastal release

## Scientific objective

Follow a passive stochastic release near the Sarno River mouth from **2021-07-01 09:00 to 15:00 UTC**. The diagnostic question is how selected particles spread horizontally and occupy the model's near-surface vertical coordinate over six hours. This is a conditional model result, not pollutant-mass exposure, an observational validation, or a search-area probability. No leeway object model is selected.

## Prerequisites and required fields

Use the serial C++17 application (or the MPI build described below) with NetCDF C++4, built as described in the [build guide](../docs/build.md). The provided Slurm wrapper requires the host's four environment modules, `high-wn`, curl, and `ncdump`. It requests one node, one CPU, 32 GiB RAM, and one hour. Data and all run outputs live under **`data/wacomm-sarno-lite/`**, which is ignored by Git; archive it separately. Reserve at least 40 GiB for ROMS files, normalized native forcing, snapshots, and gridded output.

Download the six files with hours **09, 10, 11, 13, 14, 15** from the [provider archive](https://data.meteo.uniparthenope.it/files/rms3/d03/history/2021/07/01/), using names `rms3_d03_20210701ZHH00.nc`. Each file is 2,816,026,296 bytes in the recorded run. The provider's 12:00 record is absent. The existing solver interpolates forcing between 11:00 and 13:00; no file or observational value is fabricated to fill this gap.

ROMS fields include wet/dry masks, longitude/latitude in degrees east/north, `h` and `zeta` in metres, `s_rho`/`s_w` dimensionless vertical coordinates, staggered `u`/`v` and `w` in m s⁻¹, `AKt` in m² s⁻¹, and chronological `ocean_time` in seconds since 1968-05-23 00:00:00. The supplied grid is 1,135 × 1,528 with 30 rho levels. The source GeoJSON is `sources-sarno_river.json`, at 14.466590881347654°E, 40.72813686316017°N. See [ROMS normalization](../docs/adapters.md) and [model limitations](../docs/model.md).

## Configuration and exact commands

`wacomm-sarno-lite.json` selects `dry=false`, the ROMS adapter, seed 5489, random particle transport, deterministic source positions, forward tracking, upper reflection, lower constraint, and horizontal kill. `io.save_input=true` saves normalized native forcing to `processed-6h/`; `io.save_history="nc"` saves particle snapshots to `snapshots-6h/`; `output-6h/` receives gridded particle counts. Restart loading is disabled. Source, closure, decay, and diffusion settings retain the historical example values.

From the repository root:

```bash
bash tools/run_sarno_lite.sh
```

The script downloads missing available forcing through temporary `.part` files, stages the checked-in configuration and source, copies the executable, records provenance, and submits `tools/sarno_lite_job.sh`. It refuses to overwrite existing six-hour outputs. Archive those directories before deliberately repeating the run. For an already staged configuration, the equivalent solver command, executed inside the run root, is:

```bash
./wacommplusplus wacomm-sarno-lite-6h.json
```

The configured source parameter is named `particlesPerHour`, but the current solver invokes emission once at each forcing-interval start. The two-hour gap therefore yields **five batches of 10,000**, at 09, 10, 11, 13, and 14 UTC: **50,000 emitted**, not 60,000. This scenario is not a constant hourly release through the missing interval. No governing equation or emission implementation was changed for this run.

## Expected outputs and verification

Slurm job **6210** ran on `high-wn` and the application returned **0**. It covered five intervals spanning exactly 21,600 seconds. The final 15:00 input is a boundary record and produces no extra integration interval; the corresponding warning is expected. Snapshots are labeled by their actual `particle_time` at 10, 11, 13, 14, and 15 UTC, while gridded output filenames use interval-start hours. There is no 12:00 snapshot.

| Physical snapshot (UTC) | Selected active particles |
|---|---:|
| 10:00 | 9,926 |
| 11:00 | 19,003 |
| 13:00 | 24,561 |
| 14:00 | 32,393 |
| 15:00 | 40,109 |

The final selected-member median distance from the source is **132 m**, with a 90th percentile of **262 m**. These are unweighted descriptive great-circle distances, not confidence radii. Final stored depths, after converting the positive-up coordinate to positive-down display, have median **0.206 mm** and 10–90% member range **0.051–0.479 mm**. The units expose the near-surface numerical output; they do not imply millimetre-scale physical accuracy.

Run `ctest --test-dir build --output-on-failure`. The portable and serial regression suites cover forward/backward physical intervals, restart continuity, adapters, and configuration. Relevant particle assertions use 1e-8 fractional-grid tolerance for restart position and 1e-12 s for age; these are regression tolerances, not coastal forecast error bounds. For run repetition, require identical particle counts/IDs and seed, then compare physical-time coordinates and output checksums on the archived compiler/backend. Native reuse is a separate equivalence check and has not been claimed from merely generating the files.

## Publication figures

The read-only tool produces SVG and PDF with vector labels/contours and rasterized dense point layers, plus 400 dpi PNG. It records input, script, and output checksums, software versions, selection counts, bins, and statistics in JSON. It needs Python 3.11 and the pinned packages in `tools/requirements-figures.txt`:

```bash
python3.11 -m venv data/wacomm-sarno-lite/venv
data/wacomm-sarno-lite/venv/bin/python -m pip install -r tools/requirements-figures.txt
MPLCONFIGDIR=/tmp/wacomm-mpl data/wacomm-sarno-lite/venv/bin/python \
  tools/publication_figures.py data/wacomm-sarno-lite/snapshots-6h/*.nc \
  --grid data/wacomm-sarno-lite/roms/rms3_d03_20210701Z0900.nc \
  --map-crs EPSG:4326 --extent 14.461 14.474 40.721 40.733 \
  --source 14.466590881347654 40.72813686316017 \
  --depth-bin 0.0005 --depth-unit mm \
  --note '12:00 forcing unavailable: interpolation spans 11:00–13:00 UTC; no 12:00 release batch.' \
  --output-dir docs/figures/sarno-lite
```

![Model-result maps showing age-colored particles at 10, 13, and 15 UTC near the Sarno source, with the ROMS wet/dry mask and bathymetry.](../docs/figures/sarno-lite/sarno-maps.svg)

**Figure 1 — Model result.** EPSG:4326 Plate Carrée, 14.461–14.474°E and 40.721–40.733°N, 1 July 2021. Colors encode particle age in hours; the star is the source. Geography comes solely from the downloaded ROMS grid: the 0.5 mask contour is a visual wet/dry boundary, not a surveyed coastline or the exact solver shoreline decision. Some active points appear on the shaded side because mask-contour rendering and solver boundary logic differ. Bathymetry contours are in metres. No external basemap is downloaded, and point overplotting is not a density estimator. [PDF](../docs/figures/sarno-lite/sarno-maps.pdf) · [400 dpi PNG](../docs/figures/sarno-lite/sarno-maps.png).

![Model-result profiles showing depth-bin counts at three times, the final radial depth section colored by age, and median plus 10th–90th percentile depth at each saved time.](../docs/figures/sarno-lite/sarno-profiles.svg)

**Figure 2 — Model result.** Vertical distribution uses fixed 0.5 mm bins and counts divided by bin width, without mass weighting or probability normalization. The radial section collapses azimuth and is not a directional transect. The last panel shows sample quantiles at recorded times only, not a confidence interval. The writer derives depth from bathymetry and sigma coordinates without instantaneous free-surface displacement; “model datum” avoids implying an observed depth below the instantaneous sea surface. Panel axis ranges differ to reveal the near-surface coordinates. [PDF](../docs/figures/sarno-lite/sarno-profiles.pdf) · [400 dpi PNG](../docs/figures/sarno-lite/sarno-profiles.png).

The [figure manifest](../docs/figures/sarno-lite/sarno-figure-manifest.json) contains exact statistics and input hashes. The [run record](../docs/figures/sarno-lite/run-record.json) binds these results to the submitted configuration, binary, forcing, and output checksums. The [diagnostic methods](../docs/trajectory-diagnostics.md#publication-maps-and-profiles) define estimators, units, missing data, and unsupported geometry.

## Native forcing for future runs

The saved native names are `processed-6h/ocm3_d03_20210701ZHH.nc` at hours 09, 10, 11, 13, 14, 15, without the downloaded filename's trailing minutes. To reuse them, set `ocean_model="WaComM"`, `base_path="processed-6h/"`, those basenames in `nc_inputs`, and `save_input=false`. Use separate gridded-output and snapshot roots. The staged `wacomm-sarno-lite-native-6h.json` provides that configuration. Saved windows may already contain the adjacent boundary record used for interpolation. Retain the same physical-time ordering and source/seed settings.

## Limitations, interpretation, and reproducibility

The six-hour realization depends on forcing, the missing 12:00 record, interval-based source batches, grid resolution, closures, and the documented legacy stochastic displacement. Numerical repeatability is not observational validation; the particle rate is not a contaminant mass flux. The finite member cloud and depth quantiles do not estimate calibrated probabilities or forecast confidence. These figures take their map/profile presentation cues from the OpenDrift gallery but do not run OpenDrift or establish agreement with it.

Archive Git base revision and working-tree changes, full resolved and input configurations, source/forcing/native/restart/output checksums, seed, compiler/CMake and library versions, module list, Slurm allocation and job logs, timestep and test tolerances, plotting environment, commands, and figure checksums. The run used the earlier recorded source revision with the session's build changes; the numerical C++ sources were unchanged. Run data are intentionally outside Git; the small figures and their provenance are versioned with this guide.

## Two-process MPI calculation

Build with `USE_MPI=ON`, `USE_OMP=OFF`, `USE_CUDA=OFF`, `USE_EMPI=OFF`, `USE_OPENACC=OFF`, and `WACOMM_BOOTSTRAP_PARALLEL_IO=OFF`, using the [MPI build command](../docs/build.md#mpi-build-for-the-sarno-slurm-run). Submit from the repository root:

```bash
bash tools/run_sarno_lite.sh --mpi
```

This requests `high-wn`, one node, two tasks, one CPU per task, 64 GiB total memory, and one hour. The job launches `mpirun --np "$SLURM_NTASKS" --bind-to core --report-bindings ./wacommplusplus wacomm-sarno-lite-6h.json` within the Slurm allocation. The wrapper checks the build's MPI setting. Its run root is `data/wacomm-sarno-lite/mpi2/`, with a relative symlink to the shared `../roms/` inputs. Allow additional disk space for another set of outputs and normalized forcing. Existing serial output remains available for comparison. Both modes use the same checked-in configuration, `dry=false`, seed 5489, and 09:00–15:00 UTC forcing window; the missing 12:00 record and all scientific limitations above still apply.

Job **6222** ran on `wn01` with OpenMPI 4.1.4, two ranks bound to separate cores, and application exit code **0**. OpenMPI logged an OpenFabrics device initialization warning; this successful single-node execution provides no inter-node fabric validation. All five saved particle snapshots exactly match serial job 6210 when ordered by integer identity, including inactive particles and every stored variable. Absolute and relative numeric tolerances are zero. This is backend verification for this case, not observational validation or a guarantee for every platform, rank count, or configuration. The figures above therefore also represent the matching MPI particle state.

```bash
data/wacomm-sarno-lite/venv/bin/python tools/compare_particle_snapshots.py \
  data/wacomm-sarno-lite/snapshots-6h \
  data/wacomm-sarno-lite/mpi2/snapshots-6h \
  --json docs/figures/sarno-lite/mpi2-comparison.json
```

The [comparison report](../docs/figures/sarno-lite/mpi2-comparison.json) records each physical time, stored particle count, variable list, and both input hashes. The [MPI run record](../docs/figures/sarno-lite/mpi2-run-record.json) archives configuration, dependency versions, revision, working diff, Slurm allocation, binding log, and input/output checksums. Runtime provenance remains under `data/wacomm-sarno-lite/mpi2/provenance/`; preserve it with the data. Comparison excludes global build/configuration attributes, which legitimately differ between builds; inspect provenance separately. Gridded counts and normalized forcing are archived but are outside this particle-state comparison.

## MPI/OpenMP strong scaling

**Scientific and computational question.** How does solver time for this fixed six-hour passive-release calculation change with MPI process count when OpenMP is enabled but limited to one thread per rank? This experiment changes execution resources only. It reuses the saved native forcing described above; units, source batches, seed, physical interpretation, and missing-hour limitation remain applicable.

**Prerequisites and exact commands.** Use GNU 12.2.1, OpenMPI 4.1.4, CMake 4.4.3, the same four modules, and a Release build with MPI and OpenMP enabled and CUDA disabled. Keep the six downloaded ROMS files under `data/wacomm-sarno-lite/roms/`. Downloading is completed before preparation. The one-process preparation job rebuilds the six native inputs under `data/wacomm-sarno-lite/processed-6h/`; the performance sweep then shares those files read-only. Reserve disk space for the old forcing backup, regenerated forcing, and seven sets of particle/gridded outputs. From the repository root:

```bash
module load openssl/openssl-4.0.2
module load cmake/cmake-4.4.3
module load gcc-12.2.1/ompi-4.1.4_nccl
module load nvidia/cuda-12.8.0
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
  -DUSE_MPI=ON -DUSE_OMP=ON -DUSE_CUDA=OFF \
  -DUSE_EMPI=OFF -DUSE_OPENACC=OFF -DWACOMM_BOOTSTRAP_PARALLEL_IO=OFF
cmake --build build --parallel 8
OMP_NUM_THREADS=1 ctest --test-dir build --output-on-failure
bash tools/prepare_sarno_scaling.sh
# Wait for the preparation job to finish successfully and write its checksums.
bash tools/run_sarno_scaling.sh
```

The preparation wrapper archives existing forcing as `processed-6h-before-scaling/`, stages a separate `preparation/` run, and submits one MPI process on `high-wn` with `dry=false`, ROMS input, and `save_input=true`. It writes fresh forcing directly into `processed-6h/`. Its calculation and checksum collection finish before benchmarking. The performance wrapper requires preparation exit code zero and verifies every prepared-file checksum before submission. Both wrappers refuse to overwrite their existing run/backup directories; archive them before repeating.

The performance wrapper checks Release/MPI/OpenMP/CUDA cache settings and submits six jobs with 1, 2, 4, 8, 16, and 32 MPI processes, in ascending order. Each requests `high-wn`, one exclusive node, all node memory (`--mem=0`), one CPU per task, and a one-hour limit. `afterany` dependencies serialize the jobs even if an earlier case fails; the analysis nevertheless rejects any failed or missing case. The node has 32 physical cores across two sockets. Each job uses `mpirun --map-by core --bind-to core --report-bindings`, `OMP_NUM_THREADS=1`, `OMP_THREAD_LIMIT=1`, `OMP_DYNAMIC=FALSE`, `OMP_PROC_BIND=TRUE`, and `OMP_PLACES=cores`. OpenMP support is compiled in; this sweep does not test more than one OpenMP worker per rank.

**Outputs and verification.** Each run has its own `data/wacomm-sarno-lite/scaling/pN/` root and shares the read-only `processed-6h/` files through a relative symlink. The wrapper derives the run configuration from `examples/wacomm-sarno-lite.json`, setting `ocean_model="WaComM"`, `base_path="processed-6h/"`, `nc_inputs` to `ocm3_d03_20210701ZHH.nc` for hours 09, 10, 11, 13, 14, 15, and `save_input=false`. It retains `dry=false`, all scientific settings, particle snapshots, and gridded output. No ROMS input is opened and the normalized forcing is not rewritten. The shared loader reuses already-present boundary times only after exact agreement of all stored dynamic fields; conflicts fail rather than creating duplicate intervals. The wrapper refuses an existing `scaling/` root; archive the whole directory before repeating the experiment. After all jobs complete, use the optional Python environment above:

```bash
MPLCONFIGDIR=/tmp/wacomm-mpl data/wacomm-sarno-lite/venv/bin/python \
  tools/scaling_figures.py data/wacomm-sarno-lite/scaling \
  --output-dir docs/figures/sarno-lite/scaling
```

The collector verifies identical executable, configuration, and source hashes, recorded rank/thread counts, successful exits, and exact agreement of every stored particle variable at each physical time with the one-rank baseline. Numeric tolerances are zero after integer-ID ordering, with identical missing-value masks required. Global build attributes, gridded output, and native forcing are outside this particle comparison. It refuses missing runs or nonpositive/non-finite times. This is backend verification for this workload, not observational validation.

**Timing method.** The primary measurement $T_p$ is the sum of five solver-interval durations in seconds for $p$ MPI processes (dimensionless). A barrier waits for every rank to finish loading/preparing its forcing before rank zero starts a monotonic `steady_clock` timer. The interval includes source emission, MPI scatter/gather, particle updates, and concentration/mask calculation. A second barrier after timing prevents other ranks from starting the next forcing load while rank zero is still timed. Downloading, ROMS conversion, native-file reads, initialization outside the solver interval, and snapshot/gridded writes are excluded from $T_p$. Thus forcing downloading and preparation cannot enter the measured solver duration. These barriers are timing/ordering operations and do not change trajectories, stochastic keys, physical time, or source schedules.

Speedup is $S_p=T_1/T_p$, and efficiency is $E_p=S_p/p$, displayed as percent. A secondary series uses full application elapsed time $A_p$, measured by GNU `time` around `mpirun`, with separate ratios $A_1/A_p$ and $A_1/(pA_p)$. It includes native-file reads and output writes, but still excludes the separate download/preparation job, staging, checksums, and queue wait. Both baselines use the same Release MPI/OpenMP executable as the other points; the preparation, earlier serial, and two-rank jobs are not timing baselines. Efficiency counts active ranks, not all 32 exclusively reserved cores. The [diagnostic methods](../docs/trajectory-diagnostics.md#strong-scaling-figures) define timing boundaries, estimators, and inference limits.

![Computational diagnostic showing measured solver and full-application speedup against MPI process count and the ideal linear reference.](../docs/figures/sarno-lite/scaling/sarno-speedup.svg)

**Figure 3 — Computational diagnostic.** Fixed six-hour workload, one OpenMP thread per MPI rank, exclusive `high-wn` node. Both axes are logarithmic; the dashed line is ideal $S_p=p$. Blue is the solver measurement with forcing excluded; orange is full application elapsed time with native reads and writes. Each point is one timed run. [PDF](../docs/figures/sarno-lite/scaling/sarno-speedup.pdf) · [400 dpi PNG](../docs/figures/sarno-lite/scaling/sarno-speedup.png).

![Computational diagnostic showing measured parallel efficiency in percent against MPI process count, with a 100 percent ideal reference.](../docs/figures/sarno-lite/scaling/sarno-efficiency.svg)

**Figure 4 — Computational diagnostic.** Efficiency is $100T_1/(pT_p)$ percent, with logarithmic axes. Blue uses solver time; orange uses the separately normalized full-application time. The dashed 100% reference represents ideal linear scaling, not a fitted prediction. There are no uncertainty bars because there is only one observation per process count. [PDF](../docs/figures/sarno-lite/scaling/sarno-efficiency.pdf) · [400 dpi PNG](../docs/figures/sarno-lite/scaling/sarno-efficiency.png).

**Measured results.** Preparation job **6255** rebuilt the native forcing with exit code zero. Performance jobs **6256–6261** all completed on `wn01` (two Intel Xeon Gold 5218 sockets, 32 physical cores total) with exit code zero. All five particle snapshots agree exactly across every process count; the one-process replay also matches the preparation and earlier serial results. The regenerated native-file checksums match the earlier forcing files byte-for-byte. The [preparation-to-serial comparison](../docs/figures/sarno-lite/scaling/preparation-serial-comparison.json) and [native-to-preparation comparison](../docs/figures/sarno-lite/scaling/native-preparation-comparison.json) retain the input hashes and exact variable checks.

| MPI processes | Job | Solver time (s) | Solver speedup | Solver efficiency | Application time (s) | Application speedup |
|---:|---:|---:|---:|---:|---:|---:|
| 1 | 6256 | 10.5618 | 1.000× | 100.00% | 24.86 | 1.000× |
| 2 | 6257 | 5.8360 | 1.810× | 90.49% | 20.90 | 1.189× |
| 4 | 6258 | 3.3828 | 3.122× | 78.06% | 18.80 | 1.322× |
| 8 | 6259 | 2.1635 | 4.882× | 61.02% | 21.59 | 1.151× |
| 16 | 6260 | 1.6399 | 6.440× | 40.25% | 25.38 | 0.980× |
| 32 | 6261 | 1.7583 | 6.007× | 18.77% | 35.76 | 0.695× |

**Discussion.** Solver time decreases from 10.5618 s at one process to 1.6399 s at 16 processes: **6.44× speedup and 40.25% efficiency**. At 32 processes it rises to 1.7583 s, giving **6.01× speedup and 18.77% efficiency**. Thus the best observed solver result is at 16 processes, while doubling to 32 gives no improvement in this single sweep. The falling efficiency shows that the additional ranks are not fully converted into reduced solver time. MPI overhead, fixed rank-zero concentration/mask work, memory bandwidth and socket placement are plausible contributors; the present measurements do not distinguish their individual effects or establish that the small 16-to-32 difference is repeatable.

The full application has a different optimum in this sweep: **18.80 s at four processes (1.32× speedup)**, versus 24.86 s at one and 35.76 s at 32. At 32 processes, the measured solver accounts for about 4.9% of elapsed application time. The remainder includes native-file I/O, initialization/array allocation, launch, synchronization outside the timed interval, and output. It cannot be attributed entirely to a single bottleneck. The primary blue series excludes forcing loading/preparation, so this full-runtime slowdown does not contaminate the reported solver ratios. Neither series includes downloading or ROMS-to-native regeneration.

These measurements favor 16 ranks for the solver phase and four ranks for this complete invocation, among the tested counts only. They are observations, not a statistically established optimum: each count has one sample, execution order is fixed, and cache state, turbo frequency and shared-storage behavior are uncontrolled. A replicated, randomized sweep is needed to assess variability and choose production resources confidently. The dashed ideal lines are reference limits, not expectations fitted to this small release.

**Limitations and interpretation.** This is one ascending, sequential sweep, without randomized order, replicated timings, or cache control. Every MPI process loads the full native ocean windows; particle decomposition does not distribute the environmental grid. Rank zero also computes gridded concentration and writes results. Native reads and writes affect only the secondary full-application series. MPI communication, rank-zero concentration work, and memory bandwidth can limit the primary solver series; the experiment does not isolate each individual cost. Amdahl (1967) explains the general limit imposed by serial work, but no serial fraction is fitted here. Results do not establish multi-node scaling, multi-thread OpenMP performance, a calibrated performance model, or a ranking on other hardware. MPI's OpenFabrics/UCX environment warnings are retained in the logs; successful same-node execution does not verify an inter-node transport.

**Reproducibility metadata.** The [scaling report](../docs/figures/sarno-lite/scaling/scaling-results.json) contains preparation and performance job IDs, per-interval solver times, full elapsed times, ratios, complete configuration, compiler flags and CMake cache, Git revision and working diff, staged submission scripts, CPU topology, MPI/OpenMP environment, binding records, input/output checksums, exact particle comparisons, and plot hashes. GNU Release uses `-O3 -DNDEBUG` with `-fopenmp` and no fast-math option. Preserve all runtime data separately from Git. Earlier default-build, ROMS-forcing, and pre-fix native-boundary pilots were cancelled and excluded; they remain locally archived for traceability.

## References


- Dagestad, K.-F., Röhrs, J., Breivik, Ø., and Ådlandsvik, B. (2018). OpenDrift v1.0: a generic framework for trajectory modelling. *Geoscientific Model Development*, 11, 1405–1420. [doi:10.5194/gmd-11-1405-2018](https://doi.org/10.5194/gmd-11-1405-2018).
- Montella, R., et al. (2023). A highly scalable high-performance Lagrangian transport and diffusion model for marine pollutants assessment. *Proceedings of PDP 2023*, 17–26. [doi:10.1109/PDP59025.2023.00012](https://doi.org/10.1109/PDP59025.2023.00012).

- OpenDrift Project. [Gallery](https://opendrift.github.io/gallery/index.html), [ROMS reader example](https://opendrift.github.io/gallery/example_roms_native.html), and [vertical mixing example](https://opendrift.github.io/gallery/example_vertical_mixing.html). Visualization/software provenance; not independent scientific validation.

- Amdahl, G. M. (1967). Validity of the single processor approach to achieving large scale computing capabilities. *AFIPS Conference Proceedings*, 30 (Spring Joint Computer Conference), 483–485. [doi:10.1145/1465482.1465560](https://doi.org/10.1145/1465482.1465560).
