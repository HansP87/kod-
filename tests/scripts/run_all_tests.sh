#!/bin/zsh
set -euo pipefail

workspace_dir="$(cd "$(dirname "$0")/../.." && pwd)"
venv_robot="${workspace_dir}/.venv/bin/robot"
level="all"
robot_args=()

while [[ $# -gt 0 ]]; do
  case "$1" in
    --level)
      if [[ $# -lt 2 ]]; then
        echo "Missing value for --level" >&2
        exit 1
      fi
      level="$2"
      shift 2
      ;;
    *)
      robot_args+=("$1")
      shift
      ;;
  esac
done

if [[ ! -x "$venv_robot" ]]; then
  echo "Robot executable not found: $venv_robot" >&2
  echo "Create the local virtual environment and install requirements first." >&2
  exit 1
fi

case "$level" in
  smoke)
    output_dir="${workspace_dir}/tests/results/smoke"
    suites=("${workspace_dir}/tests/robot/smoke.robot")
    ;;
  regression)
    output_dir="${workspace_dir}/tests/results/regression"
    suites=(
      "${workspace_dir}/tests/robot/adc_regression.robot"
      "${workspace_dir}/tests/robot/config_mode.robot"
    )
    ;;
  all)
    output_dir="${workspace_dir}/tests/results/all"
    suites=(
      "${workspace_dir}/tests/robot/smoke.robot"
      "${workspace_dir}/tests/robot/adc_regression.robot"
      "${workspace_dir}/tests/robot/config_mode.robot"
    )
    ;;
  *)
    echo "Unsupported test level: $level" >&2
    echo "Expected one of: smoke, regression, all" >&2
    exit 1
    ;;
esac

mkdir -p "$output_dir"

exec "$venv_robot" --outputdir "$output_dir" "${robot_args[@]}" "${suites[@]}"