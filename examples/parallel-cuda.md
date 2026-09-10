# CUDA example

## Scientific objective and prerequisites

Exercise accelerator execution for deterministic ROMS and dynamic WRF/WW3 surface-drift cases and compare them with the CPU reference. A supported Linux or Windows NVIDIA environment, CUDA toolkit, compatible driver/GPU, application dependencies, and the inputs named by the selected example are required. Modern macOS does not support CUDA.

## Configuration and command

Configure with `cmake -S . -B build-cuda -DUSE_CUDA=ON`, build with `cmake --build build-cuda`, then run `./build-cuda/wacommplusplus examples/parallel-cuda.json`. To exercise dynamic environmental fields, provide `forcing.nc`, `wrf.nc`, `ww3.nc`, and `sources.json`, then run `./build-cuda/wacommplusplus examples/sar-person-wrf-ww3-forward.json`. When no CUDA device is available the executable uses CPU execution; when a device is available it runs the CUDA kernel with the same physical-time, environmental interpolation, vector-composition, and closure configuration.

## Expected behavior and validation

Compare identity, particle count, position, health, age, drift side, closures, concentration, and output time with the serial result using declared tolerances. Run `ctest --test-dir build-cuda -R cuda_particle_parity --output-on-failure`; on a GPU this includes nonuniform dynamic wind and coupled wind/Stokes interpolation. Run compute-sanitizer where available and verify all device-to-host copies complete before host reads.

## Limitations, interpretation, and reproducibility

Do not infer measured scientific equivalence from a successful kernel launch or compile-only CI. Record revision, inputs/configuration and checksums, compiler, CUDA toolkit, driver/GPU, CMake options, launch settings, tests, tolerances, timing, and output checksums. See [parallelism](../docs/parallelism.md) and [testing](../docs/testing.md).


## References

- Dagestad, K.-F., Röhrs, J., Breivik, Ø., and Ådlandsvik, B. (2018). OpenDrift v1.0: a generic framework for trajectory modelling. *Geoscientific Model Development*, 11, 1405–1420. [doi:10.5194/gmd-11-1405-2018](https://doi.org/10.5194/gmd-11-1405-2018).
- Montella, R., et al. (2023). A highly scalable high-performance Lagrangian transport and diffusion model for marine pollutants assessment. *Proceedings of PDP 2023*, 17–26. [doi:10.1109/PDP59025.2023.00012](https://doi.org/10.1109/PDP59025.2023.00012).
