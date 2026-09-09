# CUDA example

## Scientific objective and prerequisites

Exercise accelerator execution for a deterministic ROMS case and compare it with the CPU reference. A supported Linux or Windows NVIDIA environment, CUDA toolkit, compatible driver/GPU, application dependencies, `forcing.nc`, and `sources.json` are required. Modern macOS does not support CUDA.

## Configuration and command

Configure with `cmake -S . -B build-cuda -DUSE_CUDA=ON`, build with `cmake --build build-cuda`, then run `./build-cuda/wacommplusplus examples/parallel-cuda.json`. When no CUDA device is available the executable uses CPU execution; when a device is available it runs the CUDA kernel with the same physical-time and closure configuration.

## Expected behavior and validation

For a CUDA-supported path, compare identity, particle count, position, health, age, closures, and concentration with the serial result using published tolerances. Run compute-sanitizer where available and verify all device-to-host copies complete before host reads.

## Limitations, interpretation, and reproducibility

Do not infer measured scientific equivalence from a successful kernel launch or compile-only CI. Record revision, inputs/configuration and checksums, compiler, CUDA toolkit, driver/GPU, CMake options, launch settings, tests, tolerances, timing, and output checksums. See [parallelism](../docs/parallelism.md) and [testing](../docs/testing.md).
