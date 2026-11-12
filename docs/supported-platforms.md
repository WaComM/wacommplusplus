# Supported platforms

The portable C++17 numerical core is designed for Ubuntu x86_64, Rocky Linux 8.9 x86_64, macOS Intel and Apple Silicon, Windows/MSVC x86_64, Raspberry Pi OS Bookworm ARM64, and RV64 Linux. Optional HPC dependencies are not required for the core tests. Full application builds require NetCDF C/C++ with DAP2 and DAP4 enabled on every platform; this provides one explicit remote-data contract rather than platform-dependent acceptance of URI configurations. `BUILD_APPLICATION=OFF` keeps NetCDF and remote transport outside the portable core.

Fast CI builds the portable core on Ubuntu, macOS Intel, macOS Apple Silicon, and Windows/MSVC. Separate jobs build and test the complete application on both macOS architectures, Windows/MSVC with the repository vcpkg manifest, Ubuntu, Rocky Linux 8.9, and an ARM64 Debian Bookworm proxy for Raspberry Pi OS. Ubuntu application jobs validate serial, OpenMP, MPI, and combined MPI/OpenMP configurations and inspect installed dynamic libraries. The exact Rocky Linux 8.9 job pins BaseOS, AppStream, PowerTools and Extras to the 8.9 vault, uses archived EPEL packages, and performs the full serial configure, build, CTest, install and unresolved-library check. Because EPEL 8 does not package nlohmann-json, that header-only dependency is installed from a commit-verified 3.11.3 source tag in this job.

The scheduled/manual HPC workflow compiles CUDA in an NVIDIA development container, tests OpenACC, and builds/tests the complete application under Debian Trixie riscv64 through QEMU. Separate manual workflows run deterministic and stochastic CPU/CUDA parity on a self-hosted GPU and full serial application validation on native Raspberry Pi ARM64 and RV64 boards. The native workflow checks the kernel architecture, device-tree board identity, and absence of a container before configuring, then runs the full CTest suite, installs the executable, rejects unresolved dynamic libraries, and archives machine and build evidence. Proxy, cross-compile, and QEMU jobs remain portability evidence rather than native hardware evidence. Only an archived successful native run for the released revision supports a native-hardware validation claim.

Exact commands, validation gates, and failure modes are documented in the [Rocky Linux 8.9 guide](rocky-linux.md), [Raspberry Pi OS ARM64 guide](raspberry-pi.md), and [RISC-V 64 Linux guide](riscv64-linux.md).


## References

- Sandve, G. K., Nekrutenko, A., Taylor, J., and Hovig, E. (2013). Ten simple rules for reproducible computational research. *PLoS Computational Biology*, 9, e1003285. [doi:10.1371/journal.pcbi.1003285](https://doi.org/10.1371/journal.pcbi.1003285).
- Montella, R., et al. (2023). A highly scalable high-performance Lagrangian transport and diffusion model for marine pollutants assessment. *Proceedings of PDP 2023*, 17–26. [doi:10.1109/PDP59025.2023.00012](https://doi.org/10.1109/PDP59025.2023.00012).
