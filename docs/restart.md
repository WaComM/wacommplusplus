# Restart

NetCDF restart variables use dimensions `[particles, particle_time]`; the loader reads an explicit single time slice and preserves `ncUint64` identity without conversion through `double`. Version 3 adds per-particle `object_type` and `drift_side` to the version 2 state and records `checkpoint_time`, `tracking_direction`, `random_seed`, `ocean_model`, configured Git revision, compiler, CMake backend options, configuration path, and the complete resolved JSON configuration. The loader rejects unsupported versions and configured direction, seed, or ocean-model mismatches. Version 2 remains readable for passive transport but is rejected for leeway runs because deterministic continuation requires object state. Text restart files append object type and side while retaining historical passive records.

For reproducible continuation, archive the restart checksum, forcing manifest, configuration, seed, direction, ocean model, and physical checkpoint time. Legacy restart files lack enforced direction metadata and must be verified externally.

Versioned continuation clips each forcing interval by physical time. Forward intervals ending at or before the checkpoint and backward intervals beginning at or after it are skipped before source emission, particle distribution, concentration evaluation, or output work. An interior checkpoint resumes at that physical instant; forward sources are suppressed in that partially clipped interval and resume only for interval starts at or after the checkpoint. The dependency-light tests cover forward and backward boundary/interior clipping.

Exact stochastic continuation requires the checkpoint to coincide with a completed integration substep and the forcing bracket, configured `dti`, seed, and particle identities to remain unchanged. The random key uses the absolute forcing-interval time and substep number, so a restart at such a boundary reproduces the uninterrupted stochastic state. A checkpoint inside a stochastic substep changes the step partition and is not claimed to be bitwise equivalent.

Leeway-coefficient ensemble residuals are regenerated from the archived random seed and stored particle identity rather than added to the restart schema. A randomly assigned initial crosswind side is already persisted as the resolved `drift_side` and is never resampled on loading. Because both quantities are constant in physical time, continuation is exact at any deterministic integration checkpoint provided the seed, identity, object type, crosswind side, and resolved configuration are unchanged.

## References

- Montella, R., et al. (2023). A highly scalable high-performance Lagrangian transport and diffusion model for marine pollutants assessment. *Proceedings of PDP 2023*, 17–26. [doi:10.1109/PDP59025.2023.00012](https://doi.org/10.1109/PDP59025.2023.00012).
- Thygesen, U. H. (2011). How to reverse time in stochastic particle tracking models. *Journal of Marine Systems*, 88, 159–168. [doi:10.1016/j.jmarsys.2011.03.009](https://doi.org/10.1016/j.jmarsys.2011.03.009).
