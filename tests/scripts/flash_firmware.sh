#!/bin/zsh
set -euo pipefail

workspace_dir="$(cd "$(dirname "$0")/../.." && pwd)"
programmer_cli="${STM32_PROGRAMMER_CLI:-/Applications/STMicroelectronics/STM32Cube/STM32CubeProgrammer/STM32CubeProgrammer.app/Contents/MacOs/bin/STM32_Programmer_CLI}"
firmware_path="${1:-${workspace_dir}/Debug/devADC-dk.elf}"

if [[ ! -x "$programmer_cli" ]]; then
  echo "STM32_Programmer_CLI not found at: $programmer_cli" >&2
  exit 1
fi

if [[ ! -f "$firmware_path" ]]; then
  echo "Firmware image not found: $firmware_path" >&2
  exit 1
fi

"$programmer_cli" -c port=SWD mode=UR reset=HWrst -w "$firmware_path" -rst
