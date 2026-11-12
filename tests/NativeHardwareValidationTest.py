#!/usr/bin/env python3

from pathlib import Path
import sys


root = Path(sys.argv[1])
workflow = (root / ".github/workflows/native-hardware.yml").read_text(encoding="utf-8")
preflight = (root / "tools/native_hardware_validation.sh").read_text(encoding="utf-8")

required_workflow_text = (
    "runs-on: [self-hosted, linux, ARM64, raspberry-pi]",
    "runs-on: [self-hosted, linux, riscv64, riscv-hardware]",
    "native_hardware_validation.sh raspberry-pi",
    "native_hardware_validation.sh riscv64",
    "set -euo pipefail",
    "ctest --test-dir build-native --output-on-failure",
    "ldd install-native/bin/wacommplusplus",
    "actions/upload-artifact@v4",
)
required_preflight_text = (
    'architecture=$(uname -m)',
    "/proc/device-tree/model",
    '[[ "$hardware_model" == Raspberry\\ Pi* ]]',
    "qemu|tcg",
    "/.dockerenv",
    "systemd-detect-virt --vm --quiet",
    "git_revision=$(git rev-parse HEAD)",
)

for expected in required_workflow_text:
    if expected not in workflow:
        raise SystemExit(f"Native hardware workflow is missing: {expected}")
for expected in required_preflight_text:
    if expected not in preflight:
        raise SystemExit(f"Native hardware preflight is missing: {expected}")

for guide in ("raspberry-pi.md", "riscv64-linux.md"):
    text = (root / "docs" / guide).read_text(encoding="utf-8")
    if "## References" not in text:
        raise SystemExit(f"Native hardware guide lacks References: {guide}")
    if "native_hardware_validation.sh" not in text:
        raise SystemExit(f"Native hardware guide lacks preflight command: {guide}")

print("Native hardware validation contract is complete")
