# Robot Framework Integration Tests

This project now includes a minimal hardware-in-the-loop test harness based on Robot Framework.

## Current Layout

- `tests/robot/smoke.robot`: the smoke suite entry point
- `tests/robot/adc_regression.robot`: tighter regression checks for packet contents and stable channels
- `tests/robot/resources/common.resource`: shared suite setup/teardown and common keywords
- `tests/libraries/DevBoardLibrary.py`: Python keyword library for build, flash, UART, and packet parsing
- `tests/scripts/run_smoke.sh`: stable wrapper for running the smoke suite from the repo root or VS Code tasks
- `tests/scripts/run_all_tests.sh`: unified wrapper for smoke, regression, or combined runs
- `tests/scripts/ci_run_hil.sh`: CI-oriented wrapper for a dedicated hardware runner

## What It Does

The test flow can:
- build the firmware with the existing `Debug/` make setup
- flash the board using `STM32_Programmer_CLI`
- detect the STM32 virtual COM port on macOS
- talk to the firmware over UART
- verify a boot-time automatic trigger without pressing the physical button

## Firmware Test Commands

The low-priority monitor task now accepts ASCII commands on USART1:
- `PING`
- `HELP`
- `TRIGGER`

`TRIGGER` emits `OK:TRIGGER` and then schedules the same UART packet transmission path used by the button-triggered flow.

For the default smoke suite, the monitor task emits `TEST:READY` periodically and emits `TEST:AUTO_TRIGGER` periodically before scheduling a packet transmission automatically. This keeps the smoke test independent of host-to-target UART RX reliability and avoids losing the only UART burst if the host opens the port a little late.

## Host Setup

```sh
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements-test.txt
```

## Running The Smoke Test

```sh
source .venv/bin/activate
./tests/scripts/run_smoke.sh
```

## Running The Regression Suite

```sh
source .venv/bin/activate
./tests/scripts/run_all_tests.sh --level regression
```

## Running All Suites

```sh
source .venv/bin/activate
./tests/scripts/run_all_tests.sh --level all
```

## Dry Running The Smoke Test

```sh
source .venv/bin/activate
./tests/scripts/run_smoke.sh --dryrun
```

## Dry Running All Suites

```sh
source .venv/bin/activate
./tests/scripts/run_all_tests.sh --level all --dryrun
```

You can still run Robot directly if needed:

```sh
source .venv/bin/activate
robot tests/robot/smoke.robot
```

## Useful Environment Variables

- `DEVADC_SERIAL_PORT`: force a specific serial device, for example `/dev/cu.usbmodem1103`
- `STM32_PROGRAMMER_CLI`: override the default STM32CubeProgrammer CLI path
- `HIL_TEST_LEVEL`: used by `tests/scripts/ci_run_hil.sh`; defaults to `all`
- `HIL_SERIAL_PORT`: optional CI-friendly alias for `DEVADC_SERIAL_PORT`
- `HIL_PROGRAMMER_CLI`: optional CI-friendly alias for `STM32_PROGRAMMER_CLI`

## Notes

- The smoke suite assumes the board is connected over ST-LINK and its USART1 virtual COM port is available.
- The regression suite assumes the board is in its normal idle analog state; tighten or relax the thresholds in `tests/robot/adc_regression.robot` as the hardware behavior becomes better characterized.
- Robot runs on the host Mac. It is not deployed to the STM32 target.
- If CubeMX regenerates the project, host-side files under `tests/` are unaffected.
- VS Code tasks now expose smoke, regression, and all-suite entry points using the wrapper scripts.

## CI Runner Shape

For a dedicated hardware runner, the normal shape is:
- install the Arm GNU toolchain and STM32CubeProgrammer CLI on the runner
- create the local `.venv` and install `requirements-test.txt`
- export `HIL_SERIAL_PORT` if auto-detect is not reliable on that machine
- run `./tests/scripts/ci_run_hil.sh`

The CI wrapper does not assume GitHub Actions, Jenkins, or Azure DevOps specifically. It is just a stable shell entry point that any runner can call.
