#!/bin/zsh
set -euo pipefail

workspace_dir="$(cd "$(dirname "$0")/../.." && pwd)"

exec "${workspace_dir}/tests/scripts/run_all_tests.sh" --level smoke "$@"