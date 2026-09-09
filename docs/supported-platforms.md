# Supported platforms

The portable C++17 numerical core is designed for Ubuntu x86_64, Rocky Linux 8.9 x86_64, macOS Intel and Apple Silicon, Windows/MSVC x86_64, Raspberry Pi OS Bookworm ARM64, and RV64 Linux. Optional HPC dependencies are not required for the core tests.

Fast CI builds the portable core on Ubuntu, macOS Intel, macOS Apple Silicon, and Windows/MSVC. Separate jobs build and test the complete application on both macOS architectures, Windows/MSVC with the repository vcpkg manifest, Ubuntu, Rocky Linux 8.9, and an ARM64 Debian Bookworm proxy for Raspberry Pi OS. Ubuntu application jobs validate serial, OpenMP, MPI, and combined MPI/OpenMP configurations and inspect installed dynamic libraries. The exact Rocky Linux 8.9 job pins BaseOS, AppStream, PowerTools and Extras to the 8.9 vault, uses archived EPEL packages, and performs the full serial configure, build, CTest, install and unresolved-library check. Because EPEL 8 does not package nlohmann-json, that header-only dependency is installed from a commit-verified 3.11.3 source tag in this job.

The scheduled/manual HPC workflow compiles CUDA in an NVIDIA development container, tests OpenACC, and builds/tests the complete application under Debian Trixie riscv64 through QEMU. A separate manual workflow runs deterministic and stochastic CPU/CUDA parity on a self-hosted GPU. Raspberry Pi OS and RV64 hardware claims still require recorded native validation because container/QEMU execution is not native hardware evidence. Only platforms exercised by CI or a recorded native validation run should be described in a release as validated.

Exact commands, validation gates, and failure modes are documented in the [Rocky Linux 8.9 guide](rocky-linux.md) and [RISC-V 64 Linux guide](riscv64-linux.md).
