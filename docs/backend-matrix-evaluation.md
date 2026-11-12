# Backend matrix evaluation and improvement plan (November 2026)

## Purpose and scope

This report records a verification and performance pass over every execution backend of one revision (`ffa9c18` plus the dependency-cache change below). It answers two computational questions: do all backend combinations build and pass the offline suite, and which resource configuration runs the six-hour Sarno workload fastest on each machine. Timings characterize implementation and hardware. They are not observational validation, and no physical equation, parameter, or default changed.

## Test matrix

| Machine | Builds | Result |
| --- | --- | --- |
| Apple M4 Max (12 performance + 4 efficiency cores, 64 GiB), macOS 26.6.2, AppleClang 21, Open MPI 5.0.11, libomp 23.1.2 | Serial, OpenMP, MPI, MPI+OpenMP | 35, 35, 36, and 38 tests passed; no skips (Python 3.12 with `tools/requirements-figures.txt`) |
| rainbow (Ryzen 9 9900X, RTX 4080 SUPER), GCC 13.3, Open MPI 4.1.8, CUDA 12.8 | Serial, OpenMP, MPI, MPI+OpenMP, CUDA, OpenMP+CUDA, MPI+CUDA, MPI+OpenMP+CUDA | **Pending.** Slurm job 1753 (tests, then `compute-sanitizer` memcheck) and job 1754 (benchmark, `afterany:1753`) are queued behind another user's job that holds all 24 CPUs of the only node |

The first rainbow attempt (job 1747) failed in all eight configures on a transient HTTP 504 from GitHub while fetching nlohmann/json. This is an infrastructure fault, not a code defect, but it exposed that every build tree downloads every pinned archive again. The fix is the opt-in `WACOMM_DEPENDENCY_DOWNLOAD_DIR` cache described in the [build guide](build.md): archives are fetched once, their pinned SHA-256 is verified before every reuse, and a pre-seeded directory lets configures proceed during an outage. The four Mac builds were reconfigured with the option and passed again.

## Benchmark method

The workload is the Sarno-lite six-hour run (native WaComM forcing prepared from ROMS, five forcing intervals, stochastic forward tracking, seed 5489) at a uniform release of $q$ particles/hour, $q\in\{10^5,10^6\}$. [`sarno_backend_matrix.py`](../examples/wacomm-sarno-lite/tools/sarno_backend_matrix.py) runs every (build, $p$ MPI ranks, $n$ OpenMP threads, $g$ GPUs) tuple once as a warm-up and in three rotated replicate blocks. Every sample is compared with the serial-build warm-up: particle states exactly for CPU tuples, within the documented GPU health tolerance for CUDA tuples, and every gridded variable exactly. All 144 Mac samples passed. Solver time $T$ (s) is the sum of the five rank-0 solver intervals, timed between MPI barriers; it excludes forcing input and output. Application time is the launcher wall time. The estimator is the median of the three measured samples. Speedup is $S=T_{\mathrm{serial}}/T$ and CPU efficiency is $E=S/(pn)$, both dimensionless. On macOS ranks and threads are not pinned, and the efficiency cores are slower than performance cores, so efficiency above 12 workers is not a homogeneous-core measure.

## Results on the Apple M4 Max

![Bar chart of median solver time for seventeen Mac backend tuples at 100,000 particles per hour](figures/backend-matrix/solver-time-apple-m4-max.svg)

![Line chart of CPU speedup against workers for OpenMP, MPI, and hybrid placements on the Mac](figures/backend-matrix/cpu-speedup.svg)

![Stacked bars splitting application time into solver and remaining time for the best tuple of each Mac build](figures/backend-matrix/application-breakdown.svg)

All three figures are measured model-execution diagnostics of the real Sarno forcing; the companion tables are [`results-m4max-q100000.json`](figures/backend-matrix/results-m4max-q100000.json) and [`results-m4max-q1000000.json`](figures/backend-matrix/results-m4max-q1000000.json).

| $q$ (particles/hour) | Serial $T$ (s) | OpenMP 1×8 | OpenMP 1×12 | OpenMP 1×16 | MPI 8×1 | MPI+OpenMP 4×4 |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| $10^5$ | 24.49 | 3.15 s, $E$=97% | 2.31 s, 88% | 2.24 s, 68% | 3.96 s, 77% | 2.24 s, 68% |
| $10^6$ | 237.7 | – | 23.39 s, 85% | 22.35 s, 66% | 39.41 s, 75% | 22.83 s, 65% |

Findings:

1. OpenMP scales almost ideally on the performance cores (101% at 4 threads, 97% at 8). Adding the four efficiency cores gains only 3–5%.
2. MPI alone is 20–25 points less efficient than OpenMP at equal worker count (84% at 2 ranks, 77% at 8). Rank 0's own particle block took 0.37 s of a 0.53 s interval at 8 ranks; the remainder is scatter, gather, packing, and waiting for slower ranks under the static contiguous partition.
3. Every rank holds the full forcing: 4.2 GiB resident per rank at $10^5$ and 5.8–6.5 GiB at $10^6$. Eight ranks need about 35–50 GiB, which caps the usable rank count on 64 GiB machines, and the parallel forcing reads raise 8-rank application time to 9.1 s against 4.0 s of solver.

**Best Mac configurations.** At $10^5$ particles/hour, OpenMP 1×16 (2.24 s) and MPI+OpenMP 4×4 (2.24 s) tie within their sample ranges; OpenMP 1×16 has the shortest application time (4.2 s). At $10^6$ particles/hour, OpenMP 1×16 is fastest (22.3 s, 10.6×). For throughput per core, OpenMP 1×12 on the performance cores is the efficient choice. MPI is not needed on one Mac node.

The earlier rainbow physical-core sweep ([Sarno performance evaluation](../examples/wacomm-sarno-lite/docs/performance-evaluation.md)) measured 12-thread CPU efficiency of 90–93% and one GPU 2.8× faster than twelve cores at $10^5$ and 4.8× faster at $10^6$. The MPI, MPI+CUDA, and MPI+OpenMP+CUDA rainbow tuples of this matrix are what jobs 1753/1754 will add.

## Improvement plan

Every item below is planned and has not been implemented. Physical and numerical items require the mathematical description, backend-equivalence, forward/backward, and restart evidence that the repository contract demands.

### Computational

1. **Persistent CUDA device state.** Each interval currently allocates the device forcing, copies all time records, and frees them again inside the solver interval (`CUDA HOST2DEV`). Allocate once per run and upload only the new forcing record. This should dominate GPU-time gains at small $q$.
2. **Balanced MPI decomposition.** Replace the static contiguous scatter with a cost-aware or cyclic partition by particle age and state, and parallelize the rank-0 pack/unpack loops. The particle result is invariant because each update depends only on its own state and identity-keyed random numbers. Target: MPI efficiency within 5 points of OpenMP.
3. **Shared forcing per node.** Read forcing once per node and expose it via an MPI-3 shared-memory window, or broadcast it from one reader. This removes the per-rank 4–6 GiB replica and the I/O contention.
4. **Overlap forcing input with solving.** Prefetch interval $k+1$ while interval $k$ integrates. At $10^5$ particles/hour non-solver time is already about half of OpenMP application time.
5. **Heterogeneous-core awareness.** On hybrid-core CPUs, document a performance-cores-only default and report per-core-class efficiency.

### Numerical

1. **Position-dependent zonal metric.** Evaluate $D_x$ at the particle latitude rather than on the cell's southern node line, removing the documented $\tan\phi\,\eta\,\Delta\phi$ relative bias ([model](model.md#metric-conversion-and-position-update)).
2. **Horizontal Courant check.** Add a validated warning or limiter when $|\Delta x|/D_x$ exceeds a declared fraction of a cell, with a step-convergence test.
3. **Higher-order boundary fallback.** Replace the first-order Euler fallback of the midpoint step near coasts and open boundaries with a clipped-stage midpoint, and verify the order with the sheared-current test.

### Physical

1. **Standard random-displacement vertical diffusion.** Offer $\sqrt{2K_v\Delta t}\,Z$ with the drift correction $\partial K_v/\partial z\,\Delta t$ as an opt-in operator (Visser, 1997), keeping the legacy form for reproduction of archived runs.
2. **Shoreline interaction.** Add an explicit, documented beaching/refloating parameterization as a separate opt-in boundary operator instead of relying on reflection or constraint.

## Reproducibility

Revision `ffa9c18` with the working-tree patch archived in each suite's `provenance/suite.json`; binary and CMake-cache SHA-256, forcing checksums, configuration, and source definition are archived under `data/matrix-mac/mac-q*/provenance`. Mac command:

```bash
python3 -I examples/wacomm-sarno-lite/tools/sarno_backend_matrix.py run --root data/matrix-mac/mac-q100000 --rate 100000 --blocks 3 --forcing data/wacomm-sarno-lite --prune-snapshots --build serial=BUILD/wacommplusplus --config omp:1:16:0
```

## Limitations

Single machine, one workload, three samples per tuple, unpinned threads on macOS, and no confidence intervals. The rainbow matrix and all CUDA tuples are not yet measured in this pass.

## References

- Amdahl, G. M. (1967). Validity of the single processor approach to achieving large scale computing capabilities. *AFIPS Spring Joint Computer Conference*, 30, 483–485. [doi:10.1145/1465482.1465560](https://doi.org/10.1145/1465482.1465560).
- Hoefler, T., and Belli, R. (2015). Scientific benchmarking of parallel computing systems. *Proceedings of SC15*, article 73. [doi:10.1145/2807591.2807644](https://doi.org/10.1145/2807591.2807644).
- Visser, A. W. (1997). Using random walk models to simulate the vertical distribution of particles in a turbulent water column. *Marine Ecology Progress Series*, 158, 275–281. [doi:10.3354/meps158275](https://doi.org/10.3354/meps158275).
