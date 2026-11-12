# Output publication, asynchronous I/O, and parallel file systems

WaComM++ writes three kinds of files during a run: particle histories (`io.history_root`, the restart files of [restart](restart.md)), gridded concentration counts (`io.nc_output_root`), and, when `io.save_input=true`, normalized forcing. This page describes how the first two are published. Publication is software infrastructure: it changes when and where bytes reach storage, never the particle state, the physical time, the output cadence, or the file contents. The governing equations are described in the [model](model.md); the cadence rules are in [configuration](configuration.md#output-cadence-and-history-formats).

## Publication protocol

Every history and gridded file is written in four steps:

1. The writer creates `<name>.partial`, either next to the final file or, with `io.staging_dir`, in the staging directory.
2. A staged file is copied to `<name>.partial` in the output directory, and the staged copy is deleted.
3. With `io.fsync=true`, the file data are flushed to stable storage (`fsync`, or `F_FULLFSYNC` on macOS, `_commit` on Windows).
4. The file is renamed to `<name>`; with `io.fsync=true` the parent directory is then flushed so that the rename itself is durable (not available on Windows).

A rename within one directory is atomic on POSIX file systems, Lustre, and IBM Storage Scale (GPFS). A reader, a post-processor polling the directory, or a later restart therefore sees either the previous file or the complete new file, never a truncated one. Ordering the data flush before the rename follows the crash-consistency analysis of Pillai et al. (2014): without it, a crash can leave a published name pointing to unflushed data. A failed write, including an incomplete text stream or a writer that produces no file, removes its temporary files, publishes nothing, and stops the run with an error naming the file. A `.partial` file left by a killed process is never read by WaComM++ and is replaced by the next attempt.

Earlier revisions opened the final name directly. Their outputs are byte-identical to the current ones; only the transient name differs.

## Asynchronous I/O

With `io.async=true`, rank 0 hands each history or gridded file to a single background writer thread and continues with the next forcing window. The handed-over data are immutable for the lifetime of the write:

- histories receive a copy of the particle set taken at the output time, so later integration cannot change what is written;
- gridded files own the window's concentration array and forcing adapter, which the solver no longer modifies once the window has finished.

The files are therefore identical, byte for byte, to those of `io.async=false`; the end-to-end regression checks this forward and backward (see [verification](#verification)). Physical-time decisions, history stamps, cadence, and restart semantics are made on the solver thread exactly as before. The continuous-equals-restarted contracts of [restart](restart.md) are unaffected because a restart reads only published, complete files.

The queue is bounded by `io.async_max_pending`, the number of writes that may be queued or running. When it is full, the solver waits and logs the waiting time. Each pending history holds one copy of the particle set, `sizeof(Particle)` bytes per particle (the state of the [restart](restart.md) record plus per-object bookkeeping), and each pending gridded file holds one concentration array of $4\,N_t N_s N_\eta N_\xi$ bytes, where $N_t$, $N_s$, $N_\eta$, $N_\xi$ are the time, vertical, and horizontal dimensions of the forcing window. The default of 2 keeps at most two such snapshots in addition to the live state.

NetCDF-C and HDF5 are not thread-safe unless built with a global lock, which common distributions do not enable. WaComM++ therefore serializes every NetCDF call of the process through one mutex: the writer holds it while it creates a NetCDF file, and the solver thread holds it while it opens, reads, and normalizes forcing, writes `io.save_input` output, or loads a NetCDF restart. Consequently a NetCDF write overlaps particle integration, concentration evaluation, regridding, and text output, but not the reading of the next forcing window. Text histories (`io.save_history="text"`) and GeoJSON histories do not take the lock and can overlap forcing reads as well. Let $T_r$ be the forcing read time of a window, $T_s$ its solver time, and $T_w$ the write time of the previous window's NetCDF outputs. Per window, the synchronous wall time is approximately $T_r+T_s+T_w$ and the asynchronous one approximately $T_r+\max(T_s,T_w)$, with the queue bound applying when $T_w>T_s$ persists. These are scheduling estimates, not measured performance; the [performance protocol](performance-evaluation.md) defines how to measure them.

An error in a background write is reported at the next submission or at the end of the run, whichever comes first, and stops the run. Later queued files are still written. At the end of the run, rank 0 waits for every queued write before returning, so the process exits only after all outputs are published.

Overlapping output with computation through a dedicated resource is the approach of Damaris (Dorier et al., 2012), which used dedicated cores to hide I/O jitter in an atmospheric model; WaComM++ uses one thread on rank 0 because only rank 0 holds the gathered particle state.

### MPI and OpenMP

Only rank 0 writes. The writer thread never calls MPI or OpenMP. MPI builds request `MPI_THREAD_FUNNELED` at initialization, the level that permits additional threads when only the main thread calls MPI; this also describes the existing OpenMP workers. If the library provides less, the application logs a warning when `io.async=true`. FlexMPI/EMPI keeps its own initialization. CUDA execution is unchanged: devices finish their section and particles are copied back before any output snapshot is taken.

## Parallel file systems

`io.filesystem` declares the file system of the output directories. It is explicit configuration and is never inferred from paths or mount points.

| Value | Effect |
| --- | --- |
| `posix` (default) | Atomic rename; `io.fsync` defaults to `false`. |
| `gpfs` | Atomic rename; `io.fsync` defaults to `true`. |
| `lustre` | Atomic rename; `io.fsync` defaults to `true`; optional default striping of the output directories. |

On parallel file systems, client-side write-back caching means that a completed `write` does not imply that data reached the storage servers; `io.fsync=true` makes publication durable at the cost of one synchronous flush per file. GPFS stripes every file over all its disks by block (Schmuck and Haskin, 2002), so WaComM++ sets no layout there.

On Lustre, `io.lustre_stripe_count` and `io.lustre_stripe_size` set the default layout of each output directory, the directories of `io.history_root` and `io.nc_output_root`, with `lfs setstripe -c <count> -S <size> <directory>` before the first output is created. Files created afterwards in those directories, including the `.partial` files, inherit the layout. `0` keeps the inherited layout for that parameter, `-1` stripes over all available OSTs, and the stripe size is in bytes and must be a multiple of 65536. The `lfs` client must be on `PATH`; an absent command or a non-zero exit status stops the run. The setting persists on the directory after the run. Striping requires `io.filesystem="lustre"` and is rejected on Windows.

`io.staging_dir` names a node-local directory, for example a local SSD, a burst buffer mount, or `$TMPDIR` on a compute node, where files are written before being copied to the output directory. The directory is created if absent. With `io.async=true`, the slow transfer to the shared file system happens on the writer thread. This is the staging pattern evaluated by Liu et al. (2012) for burst-buffer tiers. The staging directory must have space for the largest output and, like the output directory, must not be shared by concurrent runs writing the same names.

## Configuration

All keys belong to the `io` object of the JSON configuration and are recorded in the resolved configuration embedded in NetCDF outputs. The legacy namelist form does not expose them.

| Key | Type and unit | Default | Validation |
| --- | --- | --- | --- |
| `async` | boolean | `false` | — |
| `async_max_pending` | integer, files | `2` | $\ge 1$ |
| `filesystem` | `posix`, `lustre`, `gpfs` | `posix` | other values rejected |
| `fsync` | boolean | `false` for `posix`, otherwise `true` | — |
| `staging_dir` | path | empty (no staging) | must be creatable as a directory at run start |
| `lustre_stripe_count` | integer, OSTs | `0` (inherit) | $\ge -1$; requires `filesystem="lustre"` |
| `lustre_stripe_size` | integer, bytes | `0` (inherit) | $\ge 0$ and a multiple of 65536; requires `filesystem="lustre"` |

```json
"io": {"save_history": "nc", "history_root": "/lustre/run/history/rst_",
       "nc_output_root": "/lustre/run/conc/conc_",
       "async": true, "async_max_pending": 2,
       "filesystem": "lustre", "lustre_stripe_count": 4, "lustre_stripe_size": 4194304,
       "staging_dir": "/local/scratch/wacomm"}
```

The [asynchronous I/O example](../examples/async-io-parallel-fs/async-io-parallel-fs.md) runs this configuration on native forcing.

## Verification

- `output_writer` checks that synchronous and asynchronous modes publish byte-identical files, that no `.partial` file remains, that the queue blocks a submission when `async_max_pending` writes are outstanding and drains on flush, that a failing or file-less writer publishes nothing and is reported once, that staged files reach their destination and leave the staging directory empty, that NetCDF jobs hold the process-wide NetCDF mutex while text jobs do not, that Lustre striping runs `lfs setstripe -c 4 -S 1048576 <directory>` through a recording stand-in for `lfs` and fails when it fails, and that the configuration keys validate and round-trip.
- `application_end_to_end` runs the production executable forward and backward with `io.async=true`, `async_max_pending=1`, `filesystem="gpfs"` (so `fsync=true`), and a staging directory, the backward run restarting from the asynchronously written forward history. It requires the text histories to equal the synchronous ones byte for byte and the gridded `conc` arrays to be equal exactly, forward and backward, serially, with two OpenMP threads, and on two MPI ranks.

Neither test uses a real Lustre or GPFS mount, and neither measures I/O performance. On a parallel file system, verify a run by comparing its output checksums with a run using `io.async=false`.

## Limitations

- Outputs remain serialized through rank 0, which gathers all particles; there is no collective MPI-IO or parallel NetCDF write. For very large particle sets, the gather and the rank-0 write bandwidth bound the output rate.
- NetCDF writes do not overlap forcing reads, for the thread-safety reason above.
- A pending snapshot increases rank-0 memory as stated above.
- `io.fsync` cannot make a write durable on storage that ignores flush requests, and directory flushing is not available on Windows.
- Lustre striping depends on the `lfs` client and changes the directory layout persistently; the WaComM++ tests use a stand-in, not a Lustre mount.
- Normalized forcing written with `io.save_input=true` is still written synchronously by its own writer.

## Reproducibility

Asynchronous publication does not change scientific output, but runs should archive the `io` settings, the rank-0 memory limit, and the file system type and Lustre layout (`lfs getstripe`) with the other execution settings listed in [reproducibility](reproducibility.md). Compare output checksums, not timestamps, between synchronous and asynchronous runs.

## References

- Pillai, T. S., Chidambaram, V., Alagappan, R., Al-Kiswany, S., Arpaci-Dusseau, A. C., and Arpaci-Dusseau, R. H. (2014). All file systems are not created equal: on the complexity of crafting crash-consistent applications. *11th USENIX Symposium on Operating Systems Design and Implementation (OSDI 14)*, 433–448. [usenix.org/conference/osdi14/technical-sessions/presentation/pillai](https://www.usenix.org/conference/osdi14/technical-sessions/presentation/pillai).
- Dorier, M., Antoniu, G., Cappello, F., Snir, M., and Orf, L. (2012). Damaris: how to efficiently leverage multicore parallelism to achieve scalable, jitter-free I/O. *2012 IEEE International Conference on Cluster Computing*, 155–163. [doi:10.1109/CLUSTER.2012.26](https://doi.org/10.1109/CLUSTER.2012.26).
- Liu, N., Cope, J., Carns, P., Carothers, C., Ross, R., Grider, G., Crume, A., and Maltzahn, C. (2012). On the role of burst buffers in leadership-class storage systems. *2012 IEEE 28th Symposium on Mass Storage Systems and Technologies (MSST)*, 1–11. [doi:10.1109/MSST.2012.6232369](https://doi.org/10.1109/MSST.2012.6232369).
- Schmuck, F., and Haskin, R. (2002). GPFS: a shared-disk file system for large computing clusters. *Proceedings of the FAST 2002 Conference on File and Storage Technologies*, 231–244. [usenix.org/conference/fast-02/gpfs-shared-disk-file-system-large-computing-clusters](https://www.usenix.org/conference/fast-02/gpfs-shared-disk-file-system-large-computing-clusters).
- Montella, R., Di Luccio, D., De Vita, C. G., Mellone, G., Lapegna, M., Ortega, G., Marcellino, L., Zambianchi, E., and Giunta, G. (2023). A highly scalable high-performance Lagrangian transport and diffusion model for marine pollutants assessment. *31st Euromicro International Conference on Parallel, Distributed and Network-Based Processing*, 17–26. [doi:10.1109/PDP59025.2023.00012](https://doi.org/10.1109/PDP59025.2023.00012).
