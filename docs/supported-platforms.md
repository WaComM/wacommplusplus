# Supported platforms

The portable C++17 numerical core is designed for Ubuntu x86_64, Rocky Linux 8.9 x86_64, macOS Intel and Apple Silicon, Windows/MSVC x86_64, Raspberry Pi OS Bookworm ARM64, and RV64 Linux. Optional HPC dependencies are not required for the core tests.

Fast CI builds the portable core on Ubuntu, macOS Intel, macOS Apple Silicon, and Windows/MSVC; it also tests exact Rocky Linux 8.9, an ARM64 Debian Bookworm proxy for Raspberry Pi OS, and an RV64 cross-compile guard. Ubuntu application jobs build and test serial, OpenMP, and MPI configurations and verify the installed executable's dynamic libraries.

The scheduled/manual HPC workflow compiles CUDA in an NVIDIA development container and runs the portable core under Debian Trixie riscv64 through QEMU. These jobs validate compilation and dependency contracts; CUDA numerical execution still requires a GPU runner, and Raspberry Pi OS/native RV64 claims still require recorded native validation. Only platforms exercised by CI or a recorded native validation run should be described in a release as validated.
