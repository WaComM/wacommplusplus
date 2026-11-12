# WaComM++ improvement plan: Claude Code task brief

This note turns the [backend matrix evaluation](backend-matrix-evaluation.md#improvement-plan) into tasks that a coding agent can execute one at a time. It is written as a prompt. Open Claude Code at the repository root and ask, for example, *"Implement task C1 from docs/improvement-plan.md"*. Each task is self-contained: it states the evidence, the files, the approach, the invariants, the tests, the acceptance criteria, and the documentation that must change with it. Status: C5 (revision `59f9b91`, with fatal-error logging in `83e6337`), C1 (`32679ce`), C2 step 1 (`e2c9a21`), and atomic-free concentration counting (`aa8378f`) are implemented and were measured on `hhcpu` ([reruns](backend-matrix-evaluation.md#reruns-after-the-first-improvements)). C1 and C5 meet their acceptance criteria. C2 meets its efficiency criterion but not its time criterion, and step 2 is not implemented. Nothing else here is implemented. The evidence is computational (timings, memory, and equivalence checks on the Sarno six-hour workload), not observational validation.

## Rules that apply to every task

Read [`AGENTS.md`](../AGENTS.md) first; it overrides this note. In short:

- One physical and numerical model. Serial, OpenMP, MPI, CUDA, forward, backward, and restart execution must integrate the same equations. No backend may contain a separate physical model, and adapters normalize data only.
- Keep the existing coding style, comment tone, and granularity. Do not reformat unrelated code.
- Computational tasks (C\*) must leave every particle state and gridded output bit-identical on CPU builds. Prove it with `mpi_decomposition_parity`, `mpi_openmp_single_worker_parity`, `mpi_openmp_multi_worker_parity`, `application_end_to_end`, `restart_format`, and an equivalence-gated rerun of the Sarno backend matrix (`sarno_backend_matrix.py run`).
- Numerical (N\*) and physical (P\*) tasks change the governing configuration. Each needs a mathematical description with every symbol and unit defined, verified peer-reviewed references, regression and backend-equivalence tests, forward/backward and restart analysis, and synchronized updates to [model.md](model.md), [configuration.md](configuration.md), the affected example guides, the main README, and [references.md](references.md). New behaviour is opt-in; the legacy operator stays available, so archived runs remain reproducible.
- Physical-time decisions round to a declared resolution; never cast with `int(...)`. Persisted particle state round-trips exactly.
- Run the documentation tests (`example_documentation`, `documentation_visuals`) for every change, plus the portable core, serial application, parallel, restart, and forward/backward tests for code changes. A skipped test is not a pass; report it as one.
- Commit one task per commit, with the evidence summarized in the message.

## Measured starting point

Revision `5a49653` (before the asynchronous and collective output commits `d960e54` and `27bb3ea`), `hhcpu` partition: 2 × Intel Xeon Gold 5520+ per node, 56 cores, 239 GiB. Sarno six-hour run, stochastic forward tracking, seed 5489; medians of three samples. All samples passed the exact equivalence gate.

| Configuration | $10^5$ particles/h solver (s) | $10^6$ particles/h solver (s) | CPU efficiency |
| --- | ---: | ---: | ---: |
| Serial | 73.10 | 731.2 | 100% |
| OpenMP 1×14 | 5.44 | – | 96% |
| OpenMP 1×28 | 3.59 | 35.89 | 73% |
| OpenMP 1×56 (best on one node) | 1.89 | 18.81 | 69% |
| MPI 2×1 | 43.84 | – | 83% |
| MPI 28×1 | 4.85 | 46.30 | 54–56% |
| MPI+OpenMP 4×14 | 2.35 | 23.28 | 56% |
| 4 nodes, MPI+OpenMP 16×14 (best overall) | – | 7.43 | 44% |

Other facts the tasks rely on:

- Peak resident memory is 3.8–5.1 GiB per MPI rank, because every rank reads and holds the full forcing.
- Time outside the solver (start-up, forcing input, output) is 8.1 s for OpenMP 1×56, 19.7 s for 28 ranks, and 23.5 s for 112 ranks on four nodes.
- At two ranks, rank 0 processed its block at 23,458 particles/s against the serial average of 25,309, while the solver interval scaled to 83% efficiency.

## Execution order

C5 first: it is small and protects every later experiment. Then C1 and C2; both are now measured ([reruns](backend-matrix-evaluation.md#reruns-after-the-first-improvements)). C2 step 2 is deferred, because the hourly Sarno output gathers particle states every interval and the step could save at most part of the 0.37 s distribution time. Most of the remaining time is outside the solver (70% of the four-node 16×14 application time), and in the full-socket OpenMP efficiency loss (96% at 14 threads, 72% at 56). The next step is to measure both. Rank-0 timers separate start-up, forcing input, and output, with `io.async` off and on, and a `numactl --interleave=all` run tests C6 without code changes. Those measurements decide the order of C6, C4, and C3. Overlap (C4) can hide input only behind solver time: at most 1.8 s for OpenMP 1×56 at $10^5$ particles/hour and 5.6 s for four-node 16×14. C3 matters most with many ranks per node. C7 and C8 are independent. N1–N4 and P1–P4 are independent of the computational tasks and of each other, except that P1 and P2 share the stochastic-operator configuration design: do P1 first.

## Computational tasks

### C1. Balanced MPI particle decomposition

- **Evidence.** `Wacomm::run` in `Wacomm.cpp` splits the particle vector into contiguous blocks with `MPI_Scatterv`, and the remainder goes to rank 0. Particles are stored in emission order. Rank 0 therefore holds the oldest particles, which integrate the full interval, while recently emitted particles start part-way through it and land on the last rank. MPI efficiency is 83% at two ranks against 100% for two OpenMP threads, and every one-node hybrid placement is 24–32% slower than OpenMP 1×56. The OpenMP loop already uses `schedule(dynamic, 64)` for this reason.
- **Approach.** Assign particle $i$ to rank $i \bmod p$, or by a cost estimate (remaining physical time in the interval). Pack the send buffer in that order, and unpack the gathered buffer back to original indices, so the particle vector order after the interval is unchanged. Keep `send_counts`/`displs` consistent with the new packing. Apply the same change to the `USE_EMPI` path.
- **Invariant.** Each update depends only on the particle's own state and identity-keyed random numbers, so results must stay bit-identical for every $p$.
- **Tests.** Extend `MpiDecompositionTest.cpp` with a population in which young and old particles alternate, and assert exact equality with the serial result for $p=2,3,4$. Keep `application_end_to_end` passing at its current tolerances.
- **Acceptance.** Rerun the `hhcpu` 10⁵ suite with `hhcpu_backend_matrix_job.sh`. MPI 2×1 efficiency is at least 95%, and MPI+OpenMP 4×14 is within 5% of OpenMP 1×56. All samples pass equivalence.
- **Result.** Met at `94e9aa4` (jobs 1077483–1077485). MPI 2×1 efficiency is 99.3% and 4×1 is 98.8%. MPI+OpenMP 4×14 is 1.6% slower than OpenMP 1×56 at $10^5$ particles/hour and 1.5% at $10^6$; at `aa8378f`, after the concentration change sped up OpenMP 1×56 more, the gaps are 4.4% and 3.8%. All samples passed equivalence.
- **Documentation.** [parallelism.md](parallelism.md), the [backend matrix evaluation](backend-matrix-evaluation.md) (new results section; keep the old numbers as history), and the README performance summary.

### C2. Shrink the serial rank-0 section and keep particles resident

- **Evidence.** Every solver interval runs source emission, packing, scatter, gather, unpacking, dead-particle removal (`erase/remove_if`), and `Concentration::evaluate` on rank 0, and moves the whole population twice. Four nodes give 2.53× over one node on four times the cores (63% relative efficiency).
- **Approach.** Step 1: parallelize the pack and unpack loops with OpenMP (`#pragma omp parallel for`), matching the existing pragma style. Step 2: keep particles resident on their owning rank across intervals. Rank 0 emits and distributes only new particles. Each rank removes its own dead particles. Concentration is accumulated per rank and reduced with `MPI_Reduce` in the identical summation order (see the invariant). Particle states are gathered to rank 0 only at output and restart cadence.
- **Invariant.** Particle states stay bit-identical. Floating-point concentration sums may change summation order. If they do, either keep a deterministic order (gather counts in particle-index order) or declare and test a tolerance, and document it in [testing.md](testing.md). Exact equality is preferred. Restart files must be identical whatever the decomposition.
- **Tests.** `mpi_decomposition_parity`, `mpi_openmp_multi_worker_parity`, `restart_format`, and a new test that restarts a 3-rank run on 2 ranks and compares it with the continuous run.
- **Acceptance.** The four-node 16×14 solver time at 10⁶ particles/hour falls from 7.43 s to at most 5.5 s, and relative four-node efficiency rises above 80%.
- **Before step 2.** The [load-balance model](backend-matrix-evaluation.md#load-balance-model) predicts that C1 alone brings 16×14 to about 6.1 s. Rank-0 concentration is 0.47 s of the 7.43 s, so the dense `MPI_Reduce` above would move more data than it saves (a $3\times30\times1135\times1528$ `float` grid, about 624 MB per interval). The Sarno outputs are hourly, so states are gathered every interval anyway. Step 2 waits for the `DISTRIBUTE`/`COLLECT` timings of the rerun. FlexMPI/EMPI changes the rank count between windows, so resident particles would also need redistribution there.
- **Result.** At `aa8378f` (jobs 1077962–1077964) the four-node 16×14 solver time at $10^6$ particles/hour is 5.65 s, against 7.43 s at `5a49653` and 6.01 s at `94e9aa4`: 0.15 s above the 5.5 s target. Relative four-node efficiency is 80.3% (80.1–80.5% over the samples), just above the 80% target. Step 1's OpenMP packing left distribution and collection at 0.37 s and 0.21 s. The `omp atomic` concentration counting, 0.48 s at 16×14 and 0.79 s at OpenMP 1×56, was replaced by owner-partitioned counting with identical counts, which takes 0.09 s and 0.07 s including the land mask. Because the Sarno run gathers states every interval for its hourly output, step 2 can save at most part of the 0.37 s distribution. C3 and C4 address the 12–13 s outside the solver and are the larger remaining terms.
- **Documentation.** [parallelism.md](parallelism.md), [restart.md](restart.md), the backend matrix evaluation, and the README.

### C3. One forcing copy per node

- **Evidence.** Each rank holds 3.8–5.1 GiB. Non-solver time grows from 8.1 s (one process) to 19.7 s (28 ranks reading the same NFS files). Rank count per node is capped by memory, and `hhcpu` grants only 56 GiB unless `--mem=0` is requested.
- **Approach.** In `WacommPlusPlus.cpp`, where each rank calls `adapter->process()`, split `MPI_COMM_WORLD` with `MPI_Comm_split_type(MPI_COMM_TYPE_SHARED)`. One rank per node reads the record into an `MPI_Win_allocate_shared` window, and the other ranks map the same memory (Hoefler et al., 2013). Simpler first step: read on world rank 0 and `MPI_Bcast` the arrays. Adapters must keep validating metadata before exposing fields. The shared window holds read-only normalized fields; no adapter may change physics.
- **Invariant.** Forcing values on every rank are byte-identical to the single-reader result.
- **Tests.** A two-rank test that compares checksums of every forcing array against a serial read. `application_end_to_end` on two ranks.
- **Acceptance.** Peak node memory for 28 ranks is under 1.3× that of one rank. Non-solver time for 28 ranks is within 20% of one process.
- **Documentation.** [parallelism.md](parallelism.md), [build.md](build.md) (MPI-3 requirement), the backend matrix evaluation, and the README.

### C4. Overlap forcing input with solving

- **Evidence.** At 10⁵ particles/hour, 81% of the fastest application run (OpenMP 1×56: 1.89 s solver, 8.1 s other) is outside the solver.
- **Approach.** The timings were measured at `5a49653`, before asynchronous output (`io.async`, `OutputWriter`, revision `d960e54`). First rerun the suite with `io.async=true` to separate output from input time. Then double-buffer the ocean (and weather and wave) adapter for record $k+1$ and read it on a separate thread while interval $k$ integrates. The interval loop, the backward ordering (`ocean_time_direction`), and the restart checkpoint logic (`NumericalHelpers::activeInterval`) must stay the only places that decide which record is used.
- **Invariant.** Results are bit-identical, forward and backward, continuous and restarted.
- **Tests.** Forward and backward `application_end_to_end`, `restart_format`, and a test forcing a slow reader (an injected delay) to prove the solver waits for the record.
- **Acceptance.** For OpenMP 1×56 at 10⁵ particles/hour, non-solver time falls by at least 40%.

### C5. Fail fast on resource limits

- **Evidence.** `send_counts` and `displs` are `int`, and `(int)(particlesPerProcess + spare)` truncates silently above $2^{31}-1$ particles. 1,000,000 particles/hour sustained for 90 days is $2.16\times10^9$. On `hhcpu` the 28-rank runs were killed by the cgroup out-of-memory handler with exit 137, without any WaComM++ message.
- **Approach.** Before the scatter, if a count or displacement exceeds `INT_MAX`, stop with an explicit error naming the limit, or use MPI-4 large-count calls (`MPI_Scatterv_c`) where the library provides them, behind a compile-time check. At start-up, estimate per-rank forcing bytes from the adapter dimensions and compare ranks-per-node × bytes with the cgroup limit (`/sys/fs/cgroup/.../memory.max` or `memory.limit_in_bytes`) or `MemAvailable`. Warn above 80%, and stop with a clear message above 100% unless the user overrides.
- **Tests.** A unit test for the count guard with a mocked size. A unit test for the memory estimate against known array sizes.
- **Acceptance.** An oversubscribed launch prints a WaComM++ error before reading forcing.
- **Result.** Met at `aa8378f` (job 1078221). Under the 56 GiB default `hhcpu` grant, MPI 28×1 stopped after 1.1 s with a peak resident set of 26 MB, before reading forcing. Rank 0 logged `28 rank(s) x 4.13 GiB = 115.59 GiB against 56.00 GiB` with the cgroup file as its source, every rank logged the error, and the exit status was 1. Before `83e6337` the log line was lost when the uncaught exception ended the process.
- **Documentation.** [parallelism.md](parallelism.md) and [configuration.md](configuration.md), if an override key is added.

### C6. NUMA placement and the full-socket efficiency drop

- **Evidence.** OpenMP efficiency is 96% at 14 threads, 73% at 28 (one full socket), and 69% at 56. The rank-0 particle rate of MPI 28×1 falls by a similar fraction, so the loss is shared-resource contention, not OpenMP overhead. Forcing arrays are first-touched by the reading thread, so at 56 threads half the cores read remote memory.
- **Approach.** First measure: run `perf stat` (or LIKWID) on OpenMP 1×14, 1×28, and 1×56 to record memory bandwidth, LLC misses, and clock. Then test parallel first-touch initialization of the adapter arrays and `numactl --interleave=all`, and keep the variant only if it is faster.
- **Invariant.** Results are bit-identical.
- **Acceptance.** Either a documented 10% gain at 56 threads, or a documented counter-based explanation in the backend matrix evaluation of why there is none.

### C7. Persistent CUDA device state

- **Evidence.** Each interval allocates device forcing, copies every time record, and frees it again inside the solver interval (`CUDA HOST2DEV`, `cudaFree` block in `Wacomm.cpp`). On rainbow (RTX 4080 SUPER, job 1754) the upload is 41% of the CUDA solver time at $10^5$ particles/hour and 10% at $10^6$. With two and four MPI ranks on the one GPU, each rank repeats it over the shared PCIe link, and the per-rank upload grows from 0.47 s to 0.85 s and 1.66 s ([backend matrix evaluation](backend-matrix-evaluation.md#results-on-rainbow-cpu-and-cuda)).
- **Approach.** Allocate once per run, and upload only the new record per interval. Log a warning when more ranks than visible devices share a GPU.
- **Tests.** `cuda_particle_parity` on the CUDA host, and the GPU tolerance rules in [testing.md](testing.md).
- **Acceptance.** `CUDA HOST2DEV` time per interval falls by at least the share of unchanged records.

### C8. Heterogeneous-core awareness

On hybrid-core CPUs (Apple M-series, Intel hybrid), document a performance-cores-only default thread count and report per-core-class efficiency in the benchmark collector.

## Numerical tasks

### N1. Position-dependent zonal metric

Evaluate $D_x$ at the particle latitude instead of on the cell's southern node line, which removes the documented relative bias $\tan\phi\,\eta\,\Delta\phi$ ([model](model.md#metric-conversion-and-position-update)). Here $\phi$ is latitude (rad), $\eta$ the dimensionless northward fraction of the cell, and $\Delta\phi$ the cell's latitude span (rad). Offer it as configuration, apply it identically on CPU and CUDA, and show the 0.018 m endpoint error of `application_end_to_end` fall toward round-off.

### N2. Horizontal Courant check

Warn, or optionally limit, when the dimensionless index increment $|\Delta x|/D_x$ exceeds a declared fraction of a cell. $\Delta x$ is the substep displacement in metres and $D_x$ the local cell width in metres. Add a step-convergence test with a sheared current.

### N3. Higher-order boundary fallback

Replace the first-order Euler fallback that the midpoint step uses near coasts and open boundaries with a clipped-stage midpoint. Verify the order with the `particle_physical_interval` sheared-current case near a boundary.

### N4. Application step-convergence study

Bilinear and trilinear sampling has discontinuous derivatives at cell faces, so second order is not guaranteed on real forcing (Nordam and Duran, 2020). Run the deterministic Sarno configuration at `physics.dti` = 120, 60, 30, and 15 s. Report the endpoint separation against the 15 s run (m) and the observed order per halving, $\log_2(e_{2\Delta t}/e_{\Delta t})$. Publish the result as a figure with its data table. This is verification of the discrete operator, not validation.

## Physical tasks

### P1. Standard random-displacement vertical diffusion

Add an opt-in operator

$$
\Delta z = \frac{\partial K_v}{\partial z}\,\Delta t + \sqrt{2K_v\!\left(z+\tfrac12\frac{\partial K_v}{\partial z}\Delta t\right)\Delta t}\;Z,
$$

with $K_v$ the vertical diffusivity (m² s⁻¹), $z$ depth (m), $\Delta t$ the substep (s), and $Z$ a standard normal variate from the existing keyed generator (Visser, 1997). Keep the legacy $\sigma_k K_v c_r Z\sqrt{\Delta t/\Delta t_0}$ form as the default for archived runs. Test a well-mixed-condition case: an initially uniform particle distribution must stay uniform in a non-uniform $K_v(z)$ profile. Apply the operator identically in CUDA.

### P2. Horizontal diffusivity in physical units

Offer $\Delta x_h=\sqrt{2K_h\Delta t}\,Z$ with $K_h$ in m² s⁻¹, either constant or scale-dependent after Okubo (1971), as an alternative to the legacy displacement scale $\sigma$ (m) tied to `dti`. Document the conversion $K_h=\sigma^2/(2\Delta t_0)$ for reproducing archived runs, and test the variance growth $\langle\Delta x^2\rangle = 2K_h t$.

### P3. Time-dependent vertical coordinate

The depth mapping uses $z_w=h\,|\sigma_w|$ without free-surface elevation $\zeta$ or ROMS stretching ([model](model.md#vertical-coordinate-mapping)). Offer the full ROMS transform of $(\sigma, \zeta, h)$ for both `Vtransform` options (Shchepetkin and McWilliams, 2005) as explicit configuration, with an adapter fixture for each stretching type and an extended `vertical_coordinate_round_trip` test.

### P4. Shoreline interaction

Add an explicit, documented beaching and refloating parameterization as a separate opt-in boundary operator instead of relying on reflection or constraint. It must state its empirical basis, units, and scope; reflection stays the default.

## How to verify on the cluster

Local checks first: configure the portable core, the serial application, and the OpenMP and MPI builds, then run the full CTest suite. For computational tasks, repeat on `hhcpu`:

```bash
sbatch examples/wacomm-sarno-lite/tools/hhcpu_backend_test_job.sh
```

```bash
sbatch examples/wacomm-sarno-lite/tools/hhcpu_backend_matrix_job.sh 100000 hh-q100000 serial:1:1:0 omp:1:56:0 mpi:2:1:0 mpi:28:1:0 mpi-omp:4:14:0
```

```bash
sbatch --nodes=4 examples/wacomm-sarno-lite/tools/hhcpu_backend_matrix_job.sh 1000000 hh4-q1000000 serial:1:1:0 mpi-omp:16:14:0 mpi-omp:8:28:0
```

Compare against [`results-hhcpu.json`](figures/backend-matrix-hhcpu/results-hhcpu.json). Use a new suite root for each revision: never pool samples from different revisions.

## References

- Hoefler, T., Dinan, J., Buntinas, D., Balaji, P., Barrett, B., Brightwell, R., Gropp, W., Kale, V., and Thakur, R. (2013). MPI + MPI: a new hybrid approach to parallel programming with MPI plus shared memory. *Computing*, 95(12), 1121–1136. [doi:10.1007/s00607-013-0324-2](https://doi.org/10.1007/s00607-013-0324-2).
- Hoefler, T., and Belli, R. (2015). Scientific benchmarking of parallel computing systems. *Proceedings of SC15*, article 73. [doi:10.1145/2807591.2807644](https://doi.org/10.1145/2807591.2807644).
- Nordam, T., and Duran, R. (2020). Numerical integrators for Lagrangian oceanography. *Geoscientific Model Development*, 13(12), 5935–5957. [doi:10.5194/gmd-13-5935-2020](https://doi.org/10.5194/gmd-13-5935-2020).
- Okubo, A. (1971). Oceanic diffusion diagrams. *Deep Sea Research and Oceanographic Abstracts*, 18(8), 789–802. [doi:10.1016/0011-7471(71)90046-5](https://doi.org/10.1016/0011-7471(71)90046-5).
- Shchepetkin, A. F., and McWilliams, J. C. (2005). The regional oceanic modeling system (ROMS): a split-explicit, free-surface, topography-following-coordinate oceanic model. *Ocean Modelling*, 9(4), 347–404. [doi:10.1016/j.ocemod.2004.08.002](https://doi.org/10.1016/j.ocemod.2004.08.002).
- Visser, A. W. (1997). Using random walk models to simulate the vertical distribution of particles in a turbulent water column. *Marine Ecology Progress Series*, 158, 275–281. [doi:10.3354/meps158275](https://doi.org/10.3354/meps158275).
