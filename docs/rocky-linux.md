# Rocky Linux 8.9 build guide

WaComM++ validates the complete serial application on the exact `rockylinux:8.9` container. The validation pins Rocky repositories to the 8.9 vault so that a later Rocky 8 release cannot silently change the compiler or libraries.

## Prerequisites

Install Docker or Podman on an x86_64 host. Network access is required for the vault, archived EPEL, and the pinned nlohmann-json source. The portable core has no NetCDF or logging dependency; the full application does.

## Portable core

From the repository root, run:

```bash
docker run --rm -v "$PWD:/workspace" -w /workspace rockylinux:8.9 bash -lc '
dnf install -y gcc-c++ cmake &&
cmake -S . -B build-core -DBUILD_APPLICATION=OFF -DCMAKE_BUILD_TYPE=Release &&
cmake --build build-core --parallel &&
ctest --test-dir build-core --output-on-failure'
```

Successful output ends with all dependency-free tests passing. A compiler error normally means the host mounted the wrong checkout or the repository no longer satisfies the C++17 core contract.

## Full application

The authoritative package and vault setup is the `rocky-8-9` job in `.github/workflows/ci.yml`. Reproduce that job rather than using a moving mirror. It installs the C++ compiler, CMake, pkg-config, NetCDF C/C++4, log4cplus, and a commit-verified nlohmann-json 3.11.3 tree, then runs:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
cmake --install build --prefix install
test -x install/bin/wacommplusplus
ldd install/bin/wacommplusplus
! ldd install/bin/wacommplusplus | grep 'not found'
```

The last command is the runtime dependency gate. Do not publish the executable if it reports an unresolved library. OpenMP and MPI are optional extensions and require their corresponding development packages and CMake switches.

## Validation and reproducibility

Record the container digest, Git revision, installed RPM versions, compiler and CMake versions, configure options, CTest output, input checksums, numerical tolerances, and result checksums. The CI job validates x86_64 container execution; it does not validate a different architecture or a site-specific MPI fabric.
