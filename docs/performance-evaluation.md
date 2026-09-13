# Reproducible performance evaluation for every example

## Scientific purpose and scope

This is a fixed-workload **strong-scaling** experiment for every runnable example and use case. It evaluates computational cost while holding the scientific problem, forcing, source data, output schedule, random seed, numerical settings, binary, and physical-time interval fixed. These measurements characterize implementation and hardware behavior, not observational skill or a change in model physics. Amdahl's fixed-workload argument motivates the ideal CPU reference; actual efficiency includes serial work, communication, synchronization, memory effects, and load imbalance [Amdahl, 1967]. Repeated independent executions and complete provenance follow Hoefler and Belli (2015). Very small examples may not amortize parallel overhead; report that result rather than increasing the workload for only some configurations.

Let \(p\) be MPI processes, \(n\) OpenMP threads per process, and \(g\) GPU devices, all dimensionless. Keep the same physical simulation and use the same instrumented *solver* interval in every run. Record application wall time separately. For each configuration make at least three independent completed runs after a documented warm-up; the primary estimate \(T_{p,n,g}\) is the median of solver times in seconds. Archive every sample, including the run order; do not remove outliers without a predeclared, documented rule. Randomize or rotate configuration order within each replicate block to reduce temporal hardware drift. Use exclusive nodes, fixed CPU affinity, NUMA placement, GPU binding, clocks/power policy where controllable, and an identical node type. Record all settings and unexpected contention. Never pool runs from different machines or revisions.

## Resource matrix and estimators

Perform all three CPU sweeps; the overlapping points are one configuration and need not be rerun within a block:

| Sweep | MPI / OpenMP / GPU configurations |
| --- | --- |
| MPI | 1/1/0, 2/1/0, 4/1/0, 8/1/0, 16/1/0, 32/1/0, 64/1/0 |
| OpenMP | 1/1/0, 1/2/0, 1/4/0, 1/8/0, 1/16/0, 1/32/0 |
| Hybrid, 32 CPU workers | 1/32/0, 2/16/0, 4/8/0, 8/4/0, 16/2/0, 32/1/0 |

The trailing slash in the supplied `32/1/0/` is treated as punctuation; the resource tuple is `32/1/0`. The 64-process point requires hardware that can place 64 processes without oversubscription; if unavailable, mark the protocol incomplete and do not invent a result. Let \(T_0=T_{1,1,0}\). CPU speedup is \(S_{p,n}=T_0/T_{p,n,0}\), and CPU parallel efficiency is \(E_{p,n}=S_{p,n}/(pn)\). Both are dimensionless; display efficiency as a fraction or \(100 E\) percent with the unit labeled. The ideal CPU speedup at \(pn\) workers is \(pn\), and ideal efficiency is one. Superlinear observations are possible due to memory hierarchy; retain and investigate them rather than clipping.

Select the CPU configuration \((p^*,n^*)\) with the smallest median solver time among **all** CPU points, breaking exact ties by smaller \(p\), then smaller \(n\). This is an observed optimum for this workload and hardware, not a universal model setting. Reuse the same CPU placement and execute `p*/n*/0`, `p*/n*/1`, `p*/n*/2`, `p*/n*/3`, `p*/n*/4`. The zero-device point is the already measured selected CPU run. All nonzero device counts require distinct devices and a supported binding; record rank-to-device mapping and whether idle ranks exist. For GPU runs report incremental speedup \(T_{p^*,n^*,0}/T_{p^*,n^*,g}\). For \(g\geq1\), additionally report GPU device speedup \(G_g=T_{p^*,n^*,1}/T_{p^*,n^*,g}\) and GPU device efficiency \(G_g/g\), holding CPU resources fixed. The zero-device point has no GPU efficiency. Do **not** call this CPU parallel efficiency: a CPU thread and GPU device are not equivalent processing elements. Report GPU count, utilization and memory, PCIe/network topology, and energy if measured. A GPU point is valid only if the backend executes the intended solver kernels, rather than silently falling back to CPU.

Each plotted point must have its resource tuple, sample count, median, and observed range available in a companion machine-readable table. Produce separate CPU speedup and efficiency charts for the three sweeps, plus GPU incremental speedup and GPU device speedup/efficiency for a complete GPU sweep. Show ideal references only for the CPU charts. Avoid error bars interpreted as confidence intervals unless the sampling method and estimator justify them. Compare scientific outputs by stable particle ID and physical time, including missing-value masks and declared numerical tolerances; do not use timing to infer numerical equivalence. Test forward, backward, stochastic, and restart behavior where applicable. Incompatible output, missing samples, and invalid hardware binding invalidate a point rather than yielding a plot.

## Machine-readable run contract

The portable collector is [`tools/performance_protocol.py`](../tools/performance_protocol.py). Create one directory `p{p}_n{n}_g{g}/` per unique tuple under a run root, each with `run.json` and a local validation report. Example:

```json
{
  "mpi_processes": 1,
  "openmp_threads": 1,
  "gpu_devices": 0,
  "solver_seconds": [12.4, 12.2, 12.3],
  "exit_code": 0,
  "equivalence_passed": true,
  "validation_report": "validation.md",
  "revision": "full-git-commit",
  "configuration_sha256": "sha256-of-complete-config",
  "forcing_sha256": "sha256-of-ordered-forcing-manifest",
  "binary_sha256": "sha256-of-executable",
  "hardware_id": "stable-node-model-and-topology-id",
  "timing_scope": "solver"
}
```

The validation report must record exact commands, timestamps, scheduler job IDs, compiler and CMake flags, MPI/OpenMP/CUDA runtimes, device mappings, affinity, seed, all input/output/restart checksums, complete configuration, environment, numerical tolerances, and particle comparison. The collector checks run coverage, positive finite samples, successful exits, local validation evidence, and common workload/build/hardware identifiers. **It cannot independently prove the truth of `equivalence_passed` or the recorded hashes**; the operator must generate those from the archived files and review the validation report. Run `python3 tools/performance_protocol.py /absolute/run/root` after all CPU points; it writes `results.json`, `speedup.svg`, `cpu_efficiency.svg`, and one `codex-performance-review.md` in every run directory. Repeat after the GPU sweep for `gpu_incremental_speedup.svg` and `gpu_device_scaling.svg`. Matplotlib is required for charts; the numerical collector otherwise uses the Python standard library. Outputs remain outside the simulation output directories except for the notes colocated with their run records.

## Procedure for humans and agents

1. Choose one example and archive its scientific question, full configuration, forcing, output interval, particle count, seed, revision, compiler, dependency versions, and checksums. Prepare forcing once and reuse it read-only. Record whether the case is passive, deterministic, stochastic, forward, backward, or restart-based.
2. Build one instrumented binary with the required MPI, OpenMP, and supported CUDA backends. Run the repository's portable, serial, relevant backend, forward/backward, and restart tests. Confirm the timer scope includes the same solver work on all backends; keep setup, I/O, and forcing-preparation times separately labeled.
3. Reserve a homogeneous exclusive allocation and document topology and binding. Execute the 16 unique CPU tuples above in rotated order, with at least three independent measured runs per tuple. Keep raw stdout/stderr, exit codes, elapsed times, and scheduler records for every repetition.
4. Compare every result with the serial reference using stable identity and physical time. Create `validation.md` and `run.json` only after recording the comparison outcome. A failed run stays archived for debugging but does not enter the valid collector set.
5. Invoke the collector; inspect the charts, `results.json`, and selected \((p^*,n^*)\). The selection is based on median solver time, even if another choice has higher efficiency.
6. On the same hardware and scientific workload, measure the five GPU tuples, verify distinct device bindings and CPU/GPU numerical equivalence, update the records, and rerun the collector. If four GPUs or a supported backend are unavailable, document the limitation and do not claim a complete GPU experiment.
7. Review every generated Codex note. Give an agent the note together with the referenced run artifacts and repository context; require it to isolate generic core or example-infrastructure defects, propose minimal changes, and rerun scientific and performance checks across applicable examples and backends. Do not optimize one case by changing its physics, forcing, or workload. Archive before/after records separately.

The earlier Sarno-only scripts and figures remain historical diagnostics with their own workload and run counts. They are not automatically comparable with this shared protocol.

## References

- Amdahl, G. M. (1967). Validity of the single processor approach to achieving large scale computing capabilities. *AFIPS Spring Joint Computer Conference*, 30, 483–485. [doi:10.1145/1465482.1465560](https://doi.org/10.1145/1465482.1465560).
- Hoefler, T., and Belli, R. (2015). Scientific benchmarking of parallel computing systems: twelve ways to tell the masses when reporting performance results. *Proceedings of the International Conference for High Performance Computing, Networking, Storage and Analysis*, article 73, 1–12. [doi:10.1145/2807591.2807644](https://doi.org/10.1145/2807591.2807644).
- Montella, R., Di Luccio, D., De Vita, C. G., Mellone, G., Lapegna, M., Ortega, G., Marcellino, L., Zambianchi, E., and Giunta, G. (2023). A highly scalable high-performance Lagrangian transport and diffusion model for marine pollutants assessment. *31st Euromicro International Conference on Parallel, Distributed and Network-Based Processing*, 17–26. [doi:10.1109/PDP59025.2023.00012](https://doi.org/10.1109/PDP59025.2023.00012).
