# Restart

NetCDF restart variables use dimensions `[particles, particle_time]`; the loader reads an explicit single time slice and preserves `ncUint64` identity without conversion through `double`. Version 2 files record `checkpoint_time`, `tracking_direction`, `random_seed`, and `ocean_model`. The loader rejects unsupported versions and configured direction, seed, or ocean-model mismatches. Text restart files remain supported for compatibility.

For reproducible continuation, archive the restart checksum, forcing manifest, configuration, seed, direction, ocean model, and physical checkpoint time. Legacy restart files lack enforced direction metadata and must be verified externally.

Versioned continuation clips each forcing interval by physical time. Forward intervals ending at or before the checkpoint and backward intervals beginning at or after it are skipped; an interior checkpoint resumes at that physical instant. The dependency-light tests cover forward and backward boundary/interior clipping.
