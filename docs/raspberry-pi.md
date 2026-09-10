# Raspberry Pi OS ARM64 native validation

WaComM++ validates Raspberry Pi support on a physical 64-bit Raspberry Pi, separately from the Debian ARM64 proxy in ordinary CI. The native workflow builds and tests the complete serial application; it does not enable MPI, OpenMP, OpenACC, or CUDA and therefore makes no claim about those optional backends on Raspberry Pi.

## Hardware and operating-system prerequisites

Use a Raspberry Pi running the 64-bit Raspberry Pi OS Bookworm userspace and kernel. Confirm that the kernel and board identity are native before registering a runner:

```bash
uname -m
tr -d '\0' </proc/device-tree/model
```

The first command must report `aarch64` (or `arm64`) and the model must start with `Raspberry Pi`. A generic ARM virtual machine, container, cross-compiled binary, or QEMU system is not native Raspberry Pi evidence.

Install the GitHub Actions runner directly on the host and give it the labels `self-hosted`, `linux`, `ARM64`, and `raspberry-pi`. The runner account must be able to invoke `sudo apt-get` for the packages installed by the workflow. Do not place the runner inside Docker.

## Manual local validation

From a clean repository checkout, install dependencies and run the same gates as the workflow:

```bash
sudo apt-get update
sudo apt-get install -y cmake g++ pkg-config \
  libnetcdf-dev libnetcdf-c++4-dev liblog4cplus-dev nlohmann-json3-dev
bash tools/native_hardware_validation.sh raspberry-pi native-hardware-validation.txt
cmake -S . -B build-native -DCMAKE_BUILD_TYPE=Release
cmake --build build-native --parallel 2
ctest --test-dir build-native --output-on-failure
cmake --install build-native --prefix install-native
test -x install-native/bin/wacommplusplus
ldd install-native/bin/wacommplusplus
```

The NetCDF build must expose DAP2 and DAP4; configuration fails otherwise. Every CTest must pass, the installed executable must exist, and `ldd` must contain no `not found` entry. Lower the parallel build count if the board is memory constrained; that changes build scheduling, not the model.

## GitHub validation and evidence

Run **Native ARM64 and RISC-V hardware validation** with target `raspberry-pi`. The workflow rejects a non-ARM64 kernel, a non-Raspberry Pi device-tree model, and detectable container or virtual-machine execution before installing dependencies. It archives the board model, kernel, OS release, CPU information, Git revision and worktree state, compiler/CMake versions, package versions, command logs, dynamic-link inspection, and log checksums.

A successful artifact is build and regression evidence for that exact revision, board, operating system, dependencies, and compiler. It is not a benchmark, observational validation, or evidence for all Raspberry Pi generations. Preserve the workflow URL and downloaded artifact with release records; GitHub artifact retention alone is not a permanent scientific archive.

## References

- Sandve, G. K., Nekrutenko, A., Taylor, J., and Hovig, E. (2013). Ten simple rules for reproducible computational research. *PLoS Computational Biology*, 9, e1003285. [doi:10.1371/journal.pcbi.1003285](https://doi.org/10.1371/journal.pcbi.1003285).
- Montella, R., et al. (2023). A highly scalable high-performance Lagrangian transport and diffusion model for marine pollutants assessment. *Proceedings of PDP 2023*, 17–26. [doi:10.1109/PDP59025.2023.00012](https://doi.org/10.1109/PDP59025.2023.00012).
