# RISC-V 64 Linux build guide

WaComM++ keeps its portable C++17 numerical core buildable for 64-bit RISC-V Linux without requiring vector extensions or optional HPC libraries. CI uses both a GNU cross-compile guard and a Debian Trixie `riscv64` QEMU build/test job. A separate self-hosted workflow validates the complete serial application on physical RV64 hardware.

## Cross-compile prerequisites

On Ubuntu x86_64 install CMake and the GNU RV64 cross compiler:

```bash
sudo apt-get update
sudo apt-get install -y cmake g++-riscv64-linux-gnu
```

## Cross-compile guard

From the repository root, run:

```bash
cmake -S . -B build-riscv64 \
  -DBUILD_APPLICATION=OFF \
  -DCMAKE_SYSTEM_NAME=Linux \
  -DCMAKE_SYSTEM_PROCESSOR=riscv64 \
  -DCMAKE_CXX_COMPILER=riscv64-linux-gnu-g++
cmake --build build-riscv64
```

This verifies the C++17 and data-type portability contract but cannot execute the binaries. Inspect the compiler command and confirm that no `-march=native` or host-only library entered the build.

## QEMU build and test

Install Docker and binfmt/QEMU, then use the same Debian Trixie image as the scheduled workflow:

```bash
docker run --rm --privileged tonistiigi/binfmt --install riscv64
docker run --rm --platform linux/riscv64 \
  -v "$PWD:/workspace" -w /workspace \
  riscv64/debian:trixie-slim sh -c '
    apt-get update && apt-get install -y cmake g++ &&
    cmake -S . -B build-riscv64-qemu -DBUILD_APPLICATION=OFF &&
    cmake --build build-riscv64-qemu --parallel 2 &&
    ctest --test-dir build-riscv64-qemu --output-on-failure'
```

Successful output reports all dependency-free tests passing. An `exec format error` indicates missing binfmt registration; an unavailable platform image normally indicates that the Docker daemon or registry does not expose RV64 manifests.

## Native hardware prerequisites

Use an RV64 board whose Linux distribution supplies CMake, a C++17 compiler, NetCDF C/C++4 with DAP2 and DAP4, log4cplus, nlohmann-json, and pkg-config. Debian Trixie or a compatible `apt`-based native RV64 distribution is required by the checked-in workflow. Register the GitHub Actions runner directly on the board with labels `self-hosted`, `linux`, `riscv64`, and `riscv-hardware`; do not run it in a container or QEMU virtual machine.

The preflight requires `uname -m` to report `riscv64`, reads the board model from the device tree, and rejects QEMU/TCG identities plus detectable containers and virtual machines. This distinguishes the native evidence from the scheduled emulation job. Device-tree identity is required even on a board whose distribution omits it by default.

## Full application validation

Install dependencies and run the native gates locally with:

```bash
sudo apt-get update
sudo apt-get install -y cmake g++ pkg-config \
  libnetcdf-dev libnetcdf-c++4-dev liblog4cplus-dev nlohmann-json3-dev
bash tools/native_hardware_validation.sh riscv64 native-hardware-validation.txt
cmake -S . -B build-native -DCMAKE_BUILD_TYPE=Release
cmake --build build-native --parallel 2
ctest --test-dir build-native --output-on-failure
cmake --install build-native --prefix install-native
test -x install-native/bin/wacommplusplus
ldd install-native/bin/wacommplusplus
```

Every test must pass and `ldd` must contain no unresolved library. Run **Native ARM64 and RISC-V hardware validation** with target `riscv64` for the repository-controlled version. The workflow archives the board/kernel/OS identity, Git state, tool and dependency versions, configure/build/test/install logs, installed-library inspection, and log checksums. A successful run validates that exact hardware and software tuple; it does not establish RVV performance, validate every RV64 board, or provide observational validation of model output.

## Reproducibility record

Archive the machine or emulator identity, container digest where applicable, Git revision, compiler/binutils and CMake versions, target flags, dependency versions, configure options, CTest output, forcing/configuration checksums, seed, numerical tolerances, and result checksums. Preserve the native workflow URL and downloaded evidence artifact with a release record because workflow artifact retention is finite. QEMU validation is not evidence of native execution, performance, or optional RVV behavior.


## References

- Sandve, G. K., Nekrutenko, A., Taylor, J., and Hovig, E. (2013). Ten simple rules for reproducible computational research. *PLoS Computational Biology*, 9, e1003285. [doi:10.1371/journal.pcbi.1003285](https://doi.org/10.1371/journal.pcbi.1003285).
- Montella, R., et al. (2023). A highly scalable high-performance Lagrangian transport and diffusion model for marine pollutants assessment. *Proceedings of PDP 2023*, 17–26. [doi:10.1109/PDP59025.2023.00012](https://doi.org/10.1109/PDP59025.2023.00012).
