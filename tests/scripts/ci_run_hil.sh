#!/bin/zsh
set -euo pipefail

workspace_dir="$(cd "$(dirname "$0")/../.." && pwd)"
level="${HIL_TEST_LEVEL:-all}"

if [[ -n "${HIL_SERIAL_PORT:-}" ]]; then
  export DEVADC_SERIAL_PORT="${HIL_SERIAL_PORT}"
fi

if [[ -n "${HIL_PROGRAMMER_CLI:-}" ]]; then
  export STM32_PROGRAMMER_CLI="${HIL_PROGRAMMER_CLI}"
fi

echo "Running HIL tests at level: ${level}"

exec "${workspace_dir}/tests/scripts/run_all_tests.sh" --level "$level" "$@"