# RISC-V 64 Linux build guide

WaComM++ keeps its portable C++17 numerical core buildable for 64-bit RISC-V Linux without requiring vector extensions or optional HPC libraries. CI uses both a GNU cross-compile guard and a Debian Trixie `riscv64` QEMU build/test job.

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

## Full application and native validation

The CI jobs intentionally exercise the dependency-light core because RV64 NetCDF C++4 and log4cplus packages vary by distribution. For a full native application, build those libraries for the target ABI, configure without host paths, build, run CTest, install, and inspect the executable with `ldd`. Record a native trajectory checksum before claiming native application validation.

## Reproducibility record

Archive the machine or emulator identity, container digest, Git revision, compiler/binutils and CMake versions, target flags, dependency versions, configure options, CTest output, forcing/configuration checksums, seed, numerical tolerances, and result checksums. QEMU validation is not evidence of performance or optional RVV behavior.


## References

- Sandve, G. K., Nekrutenko, A., Taylor, J., and Hovig, E. (2013). Ten simple rules for reproducible computational research. *PLoS Computational Biology*, 9, e1003285. [doi:10.1371/journal.pcbi.1003285](https://doi.org/10.1371/journal.pcbi.1003285).
- Montella, R., et al. (2023). A highly scalable high-performance Lagrangian transport and diffusion model for marine pollutants assessment. *Proceedings of PDP 2023*, 17–26. [doi:10.1109/PDP59025.2023.00012](https://doi.org/10.1109/PDP59025.2023.00012).
