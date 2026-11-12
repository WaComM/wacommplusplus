# Asynchronous output on a parallel file system

## Scientific question

Does publishing particle histories and gridded counts asynchronously, through node-local staging and with durable flushes, leave every scientific output unchanged while removing NetCDF write time from the solver's critical path? The configuration is a passive, deterministic forward run; it selects no drift model, and the historical physics are those of the [native forward example](../forward-wacomm/README.md). Only publication changes. See [output publication](../../docs/io.md).

## Prerequisites

A WaComM++ build with NetCDF C++4 (any backend: serial, OpenMP, MPI, or CUDA; `cmake -S . -B build && cmake --build build`). Six hourly native WACOMM forcing files and a `sources.json` with explicit `emission` objects, both staged in this directory or with `io.base_path` changed accordingly. Write access to `output/` and to a node-local `staging/` directory; on a cluster, point `io.staging_dir` at the compute node's local disk (for example `$TMPDIR`) and `io.nc_output_root`/`io.history_root` at the shared file system. Python 3 with `netCDF4` and `numpy` for the comparison below.

## Required forcing fields and units

Each native file carries `ocean_time` (s since 1968-05-23 00:00:00 UTC), `s_rho` and `s_w` (dimensionless sigma), `lat_rho` and `lon_rho` (degrees north and east), `mask_rho` (1), `h` (m), `zeta` (m), `u` and `v` (m s⁻¹, Earth-relative eastward and northward), `w` (m s⁻¹), and `akt` (m² s⁻¹) on the common rho-point grid, as described in [adapters](../../docs/adapters.md). Consecutive files must be chronological so that adjacent boundary records can be appended.

## Configuration

`async-io-parallel-fs.json` writes versioned NetCDF histories (`io.save_history="nc"`) and gridded counts hourly. The publication keys are:

- `io.async=true`: rank 0 hands each output to a background writer thread and continues with the next forcing window;
- `io.async_max_pending=2`: at most two outputs queued or running, which bounds rank-0 memory to two snapshots;
- `io.filesystem="gpfs"` with `io.fsync=true`: every file is flushed to stable storage before its atomic rename to the final name;
- `io.staging_dir="staging"`: files are written locally, then copied to `output/` by the writer thread.

On Lustre, use `"filesystem": "lustre"` and add, for example, `"lustre_stripe_count": 4, "lustre_stripe_size": 4194304` to set the default layout of `output/` with `lfs setstripe` before the first file is created; the `lfs` client must be on `PATH`. On a workstation, `"filesystem": "posix"` keeps the same behavior without the default flush. None of these keys changes physics, cadence, or file contents.

## Command

```bash
mkdir -p examples/async-io-parallel-fs/output
cd examples/async-io-parallel-fs && ../../build/wacommplusplus async-io-parallel-fs.json
```

For the synchronous reference, write a copy with `io.async=false`, no staging, and separate output roots, then run it the same way:

```bash
cd examples/async-io-parallel-fs && python3 -c "import json; c=json.load(open('async-io-parallel-fs.json')); c['io'].update(async=False, staging_dir='', nc_output_root='output/sync-conc-', history_root='output/sync-history-'); json.dump(c, open('sync-reference.json','w'), indent=2)" && ../../build/wacommplusplus sync-reference.json
```

## Expected outputs

`output/history-20210701Z01.nc` to `output/history-20210701Z06.nc` and `output/conc-20210701Z00.nc` to `output/conc-20210701Z05.nc`, the same names as a synchronous run. The log reports `Output writer: mode=async max_pending=2 filesystem=gpfs fsync=true staging_dir=staging`, one `Published <file> in <s> seconds` line per output, and `Solver waited <s> seconds for the output queue` whenever the queue was full. After the run, `output/` holds no `.partial` file and `staging/` is empty.

## Verification

Compare every variable of the asynchronous and synchronous outputs; the tolerance is exact equality:

```bash
cd examples/async-io-parallel-fs && python3 -c "
import glob, netCDF4, numpy
for a in sorted(glob.glob('output/history-*.nc') + glob.glob('output/conc-*.nc')):
    s = a.replace('output/', 'output/sync-')
    with netCDF4.Dataset(a) as x, netCDF4.Dataset(s) as y:
        for name in x.variables:
            assert numpy.array_equal(numpy.ma.filled(x[name][:], 0), numpy.ma.filled(y[name][:], 0)), (a, name)
print('asynchronous outputs equal synchronous outputs')"
```

File checksums differ between the two runs because the global attribute `wacomm_configuration` records the resolved `io` settings; the variables do not. The offline `application_end_to_end` regression performs the same comparison on an analytic current, forward and backward, and requires byte-identical text histories ([application verification](../../docs/application-verification.md)); `output_writer` checks publication, queueing, failure, staging, NetCDF serialization, and Lustre striping ([testing](../../docs/testing.md)).

## Limitations

Outputs are still gathered and written by rank 0; there is no collective MPI-IO write. NetCDF writes overlap particle integration and concentration evaluation but not the reading of the next forcing window, because NetCDF-C and HDF5 are serialized process-wide. Each pending history copies the particle set on rank 0. `io.fsync` relies on storage honoring flush requests. Lustre striping changes the directory layout persistently. This example has not been timed on a parallel file system; it demonstrates equivalence, not a speedup.

## Interpretation

Equality of the outputs shows that asynchronous publication is transparent to the model for this configuration; it is numerical verification, not observational validation, and it says nothing about the accuracy of the forcing. Any wall-time difference between the two runs is an I/O scheduling effect and must be measured with the [shared performance-evaluation protocol](../../docs/performance-evaluation.md), which applies to this configuration like any other: archive a Codex performance-review note for each measured resource tuple, with scientific equivalence as the prerequisite for interpreting timing.

`physics.integrator` selects the explicit second-order midpoint step, which is the default. Results produced before this option existed used the first-order `euler` step and can be reproduced by setting `"integrator": "euler"`. See [temporal integration](../../docs/model.md#temporal-integration).

## Reproducibility

Archive the Git revision, both complete configurations, forcing and source checksums, compiler, CMake options, dependencies, platform, MPI ranks and OpenMP threads, the file system type and, on Lustre, `lfs getstripe output`, the staging device, numerical tolerances (exact), the log, and per-variable comparison output. Compare variables rather than file checksums. See [reproducibility](../../docs/reproducibility.md).

## References

- Dorier, M., Antoniu, G., Cappello, F., Snir, M., and Orf, L. (2012). Damaris: how to efficiently leverage multicore parallelism to achieve scalable, jitter-free I/O. *2012 IEEE International Conference on Cluster Computing*, 155–163. [doi:10.1109/CLUSTER.2012.26](https://doi.org/10.1109/CLUSTER.2012.26).
- Liu, N., Cope, J., Carns, P., Carothers, C., Ross, R., Grider, G., Crume, A., and Maltzahn, C. (2012). On the role of burst buffers in leadership-class storage systems. *2012 IEEE 28th Symposium on Mass Storage Systems and Technologies (MSST)*, 1–11. [doi:10.1109/MSST.2012.6232369](https://doi.org/10.1109/MSST.2012.6232369).
- Pillai, T. S., Chidambaram, V., Alagappan, R., Al-Kiswany, S., Arpaci-Dusseau, A. C., and Arpaci-Dusseau, R. H. (2014). All file systems are not created equal: on the complexity of crafting crash-consistent applications. *11th USENIX Symposium on Operating Systems Design and Implementation (OSDI 14)*, 433–448.
- Montella, R., et al. (2023). A highly scalable high-performance Lagrangian transport and diffusion model for marine pollutants assessment. *Proceedings of PDP 2023*, 17–26. [doi:10.1109/PDP59025.2023.00012](https://doi.org/10.1109/PDP59025.2023.00012).
