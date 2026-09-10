#!/usr/bin/env bash

set -euo pipefail

target=${1:-}
report=${2:-native-hardware-validation.txt}

if [[ "$target" != "raspberry-pi" && "$target" != "riscv64" ]]; then
    echo "usage: $0 {raspberry-pi|riscv64} [report-file]" >&2
    exit 2
fi

architecture=$(uname -m)
case "$target" in
    raspberry-pi)
        [[ "$architecture" == "aarch64" || "$architecture" == "arm64" ]] || {
            echo "Raspberry Pi validation requires an ARM64 kernel, found $architecture." >&2
            exit 1
        }
        model_file=/proc/device-tree/model
        [[ -r "$model_file" ]] || {
            echo "Raspberry Pi model is unavailable from $model_file." >&2
            exit 1
        }
        hardware_model=$(tr -d '\000' < "$model_file")
        [[ "$hardware_model" == Raspberry\ Pi* ]] || {
            echo "Expected Raspberry Pi hardware, found: $hardware_model" >&2
            exit 1
        }
        ;;
    riscv64)
        [[ "$architecture" == "riscv64" ]] || {
            echo "RISC-V validation requires a native riscv64 kernel, found $architecture." >&2
            exit 1
        }
        model_file=/proc/device-tree/model
        [[ -r "$model_file" ]] || model_file=/sys/firmware/devicetree/base/model
        [[ -r "$model_file" ]] || {
            echo "RISC-V board identity is unavailable from the device tree." >&2
            exit 1
        }
        hardware_model=$(tr -d '\000' < "$model_file")
        if [[ "$hardware_model" =~ [Qq][Ee][Mm][Uu] || "$hardware_model" == "riscv-virtio,qemu" ]]; then
            echo "QEMU model '$hardware_model' is not native RISC-V hardware." >&2
            exit 1
        fi
        if grep -Eqi 'qemu|tcg' /proc/cpuinfo; then
            echo "QEMU/TCG was detected in /proc/cpuinfo." >&2
            exit 1
        fi
        ;;
esac

if [[ -e /.dockerenv || -e /run/.containerenv ]]; then
    echo "Container execution is not native-hardware validation." >&2
    exit 1
fi
if command -v systemd-detect-virt >/dev/null 2>&1 && systemd-detect-virt --container --quiet; then
    echo "A container runtime was detected; run directly on the board." >&2
    exit 1
fi
if command -v systemd-detect-virt >/dev/null 2>&1 && systemd-detect-virt --vm --quiet; then
    echo "A virtual machine was detected; run directly on the board." >&2
    exit 1
fi

{
    echo "WaComM++ native hardware validation"
    echo "target=$target"
    echo "hardware_model=$hardware_model"
    echo "architecture=$architecture"
    echo "kernel=$(uname -srv)"
    echo "git_revision=$(git rev-parse HEAD)"
    echo "git_status_begin"
    git status --short
    echo "git_status_end"
    echo "os_release_begin"
    sed -n '1,80p' /etc/os-release
    echo "os_release_end"
    echo "cpuinfo_begin"
    sed -n '1,160p' /proc/cpuinfo
    echo "cpuinfo_end"
    echo "cmake_version=$(cmake --version 2>/dev/null | sed -n '1p' || echo unavailable)"
    echo "compiler_version=$(c++ --version 2>/dev/null | sed -n '1p' || echo unavailable)"
    echo "ctest_version=$(ctest --version 2>/dev/null | sed -n '1p' || echo unavailable)"
    echo "validation_utc=$(date -u +%Y-%m-%dT%H:%M:%SZ)"
} > "$report"

echo "Validated native $target hardware: $hardware_model ($architecture)"
