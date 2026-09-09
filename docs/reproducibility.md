# Reproducibility

Archive the Git commit/tag, complete configuration, forcing and restart filenames plus SHA-256 checksums, random seed, compiler and version, CMake cache/options, dependency versions, operating system and architecture, MPI implementation and ranks, OpenMP thread count and affinity, GPU/driver/CUDA details, test output, tolerances, and output checksums.

The configured seed is stable and must not be replaced by wall-clock time, rank, thread identity, or accelerator scheduling. CPU and CUDA stochastic displacement use a stateless key comprising the seed, stable particle identity, physical forcing interval start, absolute substep ordinal, and component. This makes the random variate independent of OpenMP scheduling, MPI decomposition, and CUDA scheduling. CPU and CUDA transcendental implementations may differ in their last floating-point bits, so comparison requires a declared tolerance and recorded toolchain.

Every NetCDF particle/restart file and concentration file records `wacomm_git_revision`, `wacomm_compiler`, `wacomm_cmake_options`, `wacomm_configuration_file`, and the complete resolved JSON configuration in `wacomm_configuration`. The resolved snapshot includes defaults applied by the loader, so it is the authoritative configuration record for the run. The revision is captured when CMake configures the build and is `unknown` only when the source is not inside a Git checkout. Reconfigure after changing revisions; a build directory configured at an older revision intentionally continues to identify that configured source state.

These embedded attributes do not replace an external run manifest. Before a run, archive the complete configuration and calculate SHA-256 checksums for every forcing, source, and restart input. After the run, add output checksums, dependency versions, platform and accelerator details, parallel placement, tolerances, and test results. Paths are identifiers rather than content guarantees, so publish the checksum manifest with the scientific result.

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

The command fails if a declared configuration, forcing, source, restart, output, test log, or CMake cache file is missing. Relative configuration paths are resolved with the same run working directory convention as the application. The JSON contains the complete input configuration, SHA-256 and byte size for every discovered file, Git revision and dirty state, selected CMake cache entries, operating system and architecture, relevant MPI/OpenMP/CUDA environment variables, and declared tolerances. Review dependency and GPU/driver details separately when those are not represented in the CMake cache or environment.

## References

- Peng, R. D. (2011). Reproducible research in computational science. *Science*, 334, 1226–1227. [doi:10.1126/science.1213847](https://doi.org/10.1126/science.1213847).
- Sandve, G. K., Nekrutenko, A., Taylor, J., and Hovig, E. (2013). Ten simple rules for reproducible computational research. *PLoS Computational Biology*, 9, e1003285. [doi:10.1371/journal.pcbi.1003285](https://doi.org/10.1371/journal.pcbi.1003285).
