# Forward restart example

## Scientific objective

Continue a deterministic forward trajectory from a physical checkpoint and verify that it matches an uninterrupted run.

## Prerequisites and configuration

Provide the original ROMS forcing and `checkpoint-forward.nc` written by the same model configuration. The restart must contain version, physical checkpoint time, forward direction, ocean model, random seed, and 64-bit particle identities. Sources are disabled to avoid re-emission.

## Run and expected behavior

Build with `cmake -S . -B build && cmake --build build`, then run `./build/wacommplusplus examples/restart-forward.json`. Intervals ending before the checkpoint are skipped and an interval containing it resumes at the checkpoint.

## Validation, limitations, and interpretation

Run the same case continuously and compare final identity, position, health, age, particle count, and output checksum within the declared tolerance. A direction mismatch must fail. Exact continuation also requires identical forcing, configuration, seed, and substep policy.

## Reproducibility

Archive revision, full configuration, forcing and restart checksums, compiler/dependencies, CMake options, platform/backend settings, tolerances, tests, and output checksums. See [restart](../docs/restart.md) and [testing](../docs/testing.md).
