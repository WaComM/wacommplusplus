# Collective MPI-IO and parallel NetCDF output

## Scientific question

Does writing particle histories and gridded counts collectively from every MPI rank leave every scientific output unchanged with respect to rank-0 output? The configuration is a passive, deterministic forward run with the physics of the [native forward example](../forward-wacomm/README.md); it selects no drift model. Only how files are written changes. See [collective writes](../../docs/io.md#collective-writes).

## Prerequisites

A WaComM++ build with `USE_MPI=ON` and a NetCDF-C built with parallel HDF5. Without a system parallel NetCDF, use the private stack:

```bash
cmake -S . -B build-pario -DCMAKE_BUILD_TYPE=Release -DUSE_MPI=ON -DWACOMM_BOOTSTRAP_PARALLEL_IO=ON
cmake --build build-pario --parallel
```

The configure output must contain `WaComM++ collective output (io.parallel_write=collective)=ON`; other builds reject this configuration at load time. An MPI launcher (`mpiexec`), six hourly native WACOMM forcing files, and a `sources.json` with explicit `emission` objects staged in this directory, or `io.base_path` changed accordingly. Python 3 with `netCDF4` and `numpy` for the comparison below.

## Required forcing fields and units

Each native file carries `ocean_time` (s since 1968-05-23 00:00:00 UTC), `s_rho` and `s_w` (dimensionless sigma), `lat_rho` and `lon_rho` (degrees north and east), `mask_rho` (1), `h` (m), `zeta` (m), `u` and `v` (m s⁻¹, Earth-relative eastward and northward), `w` (m s⁻¹), and `akt` (m² s⁻¹) on the common rho-point grid, as described in [adapters](../../docs/adapters.md). Consecutive files must be chronological so that adjacent boundary records can be appended.

## Configuration

`collective-io-mpi.json` writes versioned NetCDF histories (`io.save_history="nc"`) and gridded counts hourly. `io.parallel_write="collective"` makes every rank write its contiguous block of particles, or of `eta_rho` rows of `conc`, with parallel NetCDF-4 in collective mode; rank 0 creates each file with the serial schema writer. `io.fsync=true` flushes each file on every rank before rank 0 renames it from `<name>.partial`. `io.mpiio_hints` passes `romio_cb_write=enable` to MPI-IO. With `"save_history": "text"`, histories are written with `MPI_File_write_at_all` instead. On Lustre, set `"filesystem": "lustre"` and `"lustre_stripe_count"` so that the output directory layout spreads the file over storage targets. The option cannot be combined with `io.async=true`, `io.staging_dir`, or GeoJSON histories.

## Command

```bash
mkdir -p examples/collective-io-mpi/output
cd examples/collective-io-mpi && mpiexec -n 4 ../../build-pario/wacommplusplus collective-io-mpi.json
```

For the rank-0 reference, write a copy with `io.parallel_write="none"`, no hints, and separate output roots, then run it with the same launcher:

```bash
cd examples/collective-io-mpi && python3 -c "import json; c=json.load(open('collective-io-mpi.json')); c['io'].update(parallel_write='none', nc_output_root='output/rank0-conc-', history_root='output/rank0-history-'); c['io'].pop('mpiio_hints'); json.dump(c, open('rank0-reference.json','w'), indent=2)" && mpiexec -n 4 ../../build-pario/wacommplusplus rank0-reference.json
```

## Expected outputs

`output/history-20210701Z01.nc` to `output/history-20210701Z06.nc` and `output/conc-20210701Z00.nc` to `output/conc-20210701Z05.nc`, with the same names as rank-0 output. The log reports one `Published <file> collectively in <s> seconds` line per output, and `output/` holds no `.partial` file after the run.

## Verification

Compare every variable of the collective and rank-0 outputs; the tolerance is exact equality:

```bash
cd examples/collective-io-mpi && python3 -c "
import glob, netCDF4, numpy
for a in sorted(glob.glob('output/history-*.nc') + glob.glob('output/conc-*.nc')):
    s = a.replace('output/', 'output/rank0-')
    with netCDF4.Dataset(a) as x, netCDF4.Dataset(s) as y:
        for name in x.variables:
            assert numpy.array_equal(numpy.ma.filled(x[name][:], 0), numpy.ma.filled(y[name][:], 0)), (a, name)
print('collective outputs equal rank-0 outputs')"
```

File checksums differ because the `wacomm_configuration` attribute records the resolved `io` settings and the HDF5 layout of a parallel write can differ; the variables do not. The `parallel_output_*_ranks` tests check the writers on 1–3 ranks against the serial writers, and `application_end_to_end` runs the executable collectively on two ranks, forward and backward, with byte-identical text histories ([testing](../../docs/testing.md)).

## Limitations

The solver still gathers the particles on rank 0 in every interval, and each collective output adds a scatter from rank 0. Rank 0 writes the NetCDF schema and the coordinate variables. Writes are synchronous. MPI-IO striping hints apply only to text histories, because NetCDF files are created by rank 0. This example demonstrates equivalence, not a speedup; it has not been timed on a parallel file system.

## Interpretation

Equality of the outputs shows that collective writing is transparent to the model for this configuration; it is numerical verification, not observational validation, and it says nothing about the accuracy of the forcing. Wall-time differences are I/O effects and must be measured with the [shared performance-evaluation protocol](../../docs/performance-evaluation.md), which applies to this configuration like any other: archive a Codex performance-review note for each measured resource tuple, with scientific equivalence as the prerequisite for interpreting timing.

`physics.integrator` selects the explicit second-order midpoint step, which is the default. Results produced before this option existed used the first-order `euler` step and can be reproduced by setting `"integrator": "euler"`. See [temporal integration](../../docs/model.md#temporal-integration).

## Reproducibility

Archive the Git revision, both complete configurations, forcing and source checksums, compiler, CMake options (including `WACOMM_BOOTSTRAP_PARALLEL_IO`), MPI implementation and version, HDF5 and NetCDF-C versions, rank count and placement, file system type and Lustre layout (`lfs getstripe output`), MPI-IO hints, numerical tolerances (exact), the log, and per-variable comparison output. See [reproducibility](../../docs/reproducibility.md).

## References

- Thakur, R., Gropp, W., and Lusk, E. (1999). Data sieving and collective I/O in ROMIO. *Proceedings of Frontiers '99, the Seventh Symposium on the Frontiers of Massively Parallel Computation*, 182–189. [doi:10.1109/FMPC.1999.750599](https://doi.org/10.1109/FMPC.1999.750599).
- Li, J., Liao, W.-k., Choudhary, A., Ross, R., Thakur, R., Gropp, W., Latham, R., Siegel, A., Gallagher, B., and Zingale, M. (2003). Parallel netCDF: a high-performance scientific I/O interface. *Proceedings of the 2003 ACM/IEEE Conference on Supercomputing (SC'03)*, 39. [doi:10.1145/1048935.1050189](https://doi.org/10.1145/1048935.1050189).
- Montella, R., et al. (2023). A highly scalable high-performance Lagrangian transport and diffusion model for marine pollutants assessment. *Proceedings of PDP 2023*, 17–26. [doi:10.1109/PDP59025.2023.00012](https://doi.org/10.1109/PDP59025.2023.00012).
