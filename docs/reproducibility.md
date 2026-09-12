# Reproducibility

Archive the Git commit/tag, complete configuration, forcing and restart filenames plus SHA-256 checksums, random seed, compiler and version, CMake cache/options, dependency versions, operating system and architecture, MPI implementation and ranks, OpenMP thread count and affinity, GPU/driver/CUDA details, test output, tolerances, and output checksums.

For every observational-calibration region also archive the immutable observation dataset, its checksum and DOI, object classification and forcing-system version, selection and missing-data rules, estimator implementation/version, sample size, observation period, spatial support, fitted covariance, held-out validation protocol and scores, and boundary-sensitivity analysis. The configuration records the minimum provenance needed to identify the estimate; it does not replace the study archive.

The configured seed is stable and must not be replaced by wall-clock time, rank, thread identity, or accelerator scheduling. CPU and CUDA stochastic displacement, wind-error, and jibing draws use stateless keys comprising the seed, stable particle identity, physical forcing interval start, absolute substep ordinal, and disjoint process component. Fixed leeway residuals use a separate identity key. This makes each random variate independent of OpenMP scheduling, MPI decomposition, and CUDA scheduling. CPU and CUDA transcendental implementations may differ in their last floating-point bits, so comparison requires a declared tolerance and recorded toolchain.

Every NetCDF particle/restart file and concentration file records `wacomm_git_revision`, `wacomm_compiler`, `wacomm_cmake_options`, `wacomm_configuration_file`, and the complete resolved JSON configuration in `wacomm_configuration`; PROJ-enabled builds also record `wacomm_proj_version`. The CMake metadata includes `WACOMM_BOOTSTRAP_DEPENDENCIES` and `WACOMM_BOOTSTRAP_PARALLEL_IO`, distinguishing installed, forced-private, and MPI-enabled private I/O builds. The resolved snapshot includes defaults applied by the loader, so it is the authoritative configuration record for the run. The revision is captured when CMake configures the build and is `unknown` only when the source is not inside a Git checkout. Reconfigure after changing revisions; a build directory configured at an older revision intentionally continues to identify that configured source state. Archive the separately versioned PROJ coordinate database and any transformation-grid checksums for projected runs because the library version alone does not identify those resources.

These embedded attributes do not replace an external run manifest. Before a run, archive the complete configuration and calculate SHA-256 checksums for every ocean, weather, wave, source, and restart input. After the run, add output checksums, dependency versions, platform and accelerator details, parallel placement, tolerances, and test results. Paths are identifiers rather than content guarantees, so publish the checksum manifest with the scientific result.

Native Raspberry Pi ARM64 and RV64 platform claims also require the evidence artifact produced by `.github/workflows/native-hardware.yml` for the claimed Git revision. Archive its device-tree model, kernel and OS identity, compiler and dependency versions, configure/build/test/install logs, dynamic-link report, checksums, and workflow URL outside finite CI artifact retention. ARM64 proxy, cross-compile, and QEMU records must remain labelled as such and cannot substitute for native-board execution.

A remote URI identifies a service response, not immutable forcing content. Lazy access does not weaken the checksum requirement: for a reproducible production run, materialize the exact remote ocean, weather, and wave datasets, record retrieval time and service URI, calculate their checksums, and run against that archived copy. HTTP validators and a successful OPeNDAP integration test establish transport behavior but do not identify scientific content. Do not claim reproducibility from a URI alone.

After the outputs and test log exist, create the machine-readable manifest from the same working directory used for the simulation:

```bash
python3 tools/reproducibility_manifest.py \
  --config examples/forward-roms.json \
  --build-dir build \
  --source-dir . \
  --output output.nc \
  --test-log ctest.log \
  --absolute-tolerance 1e-10 \
  --relative-tolerance 1e-8 \
  --manifest run-manifest.json
```

The command fails if a declared configuration, local forcing, source, restart, output, test log, or CMake cache file is missing. Relative ocean, weather, and wave paths are resolved against `io.base_path`; provider-specific remote bases must first be materialized and represented by their archived local copies because the manifest deliberately does not fetch mutable remote content. Other relative configuration paths use the run working directory convention. The JSON contains the complete input configuration, SHA-256 and byte size for every discovered file, Git revision and dirty state, selected CMake cache entries, operating system and architecture, relevant MPI/OpenMP/CUDA environment variables, and declared tolerances. Review dependency and GPU/driver details separately when those are not represented in the CMake cache or environment.

The [trajectory diagnostics](trajectory-diagnostics.md) workflow records the SHA-256 checksum and embedded build provenance of every input snapshot in its JSON product. Because absolute paths are identifiers, relocating inputs can change JSON bytes without changing diagnostic values. Archive both the diagnostic JSON and SVG with the run manifest; neither replaces the simulation outputs from which it was derived.

For the [MPI Sarno run](../examples/wacomm-sarno-lite.md#two-process-mpi-calculation), the wrapper preserves serial output and stages the two-rank run under `data/wacomm-sarno-lite/mpi2/`. It records the Slurm allocation, MPI implementation, core bindings, application exit status, complete input/output checksums, build cache, revision, and working diff. The versioned MPI run record and exact particle comparison retain verification evidence without adding the large forcing/output files to Git. Matching numerical values do not imply matching file checksums because global build metadata differs.

The [Sarno strong-scaling workflow](../examples/wacomm-sarno-lite.md#mpiopenmp-strong-scaling) archives each rank count separately under `data/wacomm-sarno-lite/scaling/pN/`. Record full-node exclusivity, active MPI rank count, one OpenMP thread, core binding, sequential execution order, solver-interval and full-application timing scopes, and the lack of timing replicates. The one-rank baseline uses the same MPI/OpenMP executable as every other point. A separate one-process job regenerates `processed-6h/` first; its checked native files are shared read-only. Neither solver nor full-application benchmark time includes this preparation or downloading. Preserve `scaling/provenance/`, per-run provenance and logs, and `scaling-results.json`; source, configuration, forcing and output hashes connect performance measurements to the fixed scientific workload. Repeat the entire sweep in a newly archived run root to estimate timing variability; do not mix measurements from different builds or protocols.

The one-process OpenMP comparison stages `data/wacomm-sarno-lite/openmp-scaling/tN/`, shares the checksum-verified native forcing, and requires a byte-identical executable to the MPI baseline. Archive Slurm CPUs per task, MPI process core mask, `OMP_NUM_THREADS`, `OMP_THREAD_LIMIT`, `OMP_DYNAMIC`, `OMP_PROC_BIND`, `OMP_PLACES`, and runtime affinity diagnostics. Use each sweep's own baseline and retain absolute times when comparing the historical MPI results; one sample per count cannot quantify run-to-run variability. See the [OpenMP guide](../examples/wacomm-sarno-lite.md#openmp-strong-scaling-and-mpi-comparison).

## References

- Peng, R. D. (2011). Reproducible research in computational science. *Science*, 334, 1226–1227. [doi:10.1126/science.1213847](https://doi.org/10.1126/science.1213847).
- Sandve, G. K., Nekrutenko, A., Taylor, J., and Hovig, E. (2013). Ten simple rules for reproducible computational research. *PLoS Computational Biology*, 9, e1003285. [doi:10.1371/journal.pcbi.1003285](https://doi.org/10.1371/journal.pcbi.1003285).
