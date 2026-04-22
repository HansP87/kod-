# Robot Framework Integration Tests

This project now includes a minimal hardware-in-the-loop test harness based on Robot Framework.

## Current Layout

- `tests/robot/smoke.robot`: the smoke suite entry point
- `tests/robot/adc_regression.robot`: tighter regression checks for packet contents and stable channels
- `tests/robot/config_mode.robot`: command/config-mode integration tests, including MCU serial-number retrieval
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
- verify the boot banner, the compact binary streaming path, and the config protocol

## Firmware Test Commands

The low-priority monitor task now accepts ASCII commands on USART1:
- `PING`
- `HELP`
- `TRIGGER`

The firmware also accepts the plain-text command `CONFIGMODE` to enter configuration mode. In configuration mode the periodic streaming path is disabled, `ENTERED CONFIGMODE` is sent back, and framed commands use this format:

```text
@,command_name_lower,parameter0,...,crc8
```

Responses use:

```text
!,COMMAND_NAME_UPPER,response_parameter0,...,crc8
```

CRC-8 uses polynomial `0x07` with seed `0xA5`, calculated over the ASCII payload before the final CRC field.

On every boot, before the regular binary stream starts, the firmware emits a startup banner during the boot phase:

```text
STARTUP:MCU_SERIAL=<24 hex chars>
```

This is the boot-ready marker used by the HIL tests.

Currently implemented framed commands:

- `mcu_serial` returns the 96-bit STM32 unique ID as 24 uppercase hexadecimal characters.
- `product_name` returns the active product name, or updates it when one parameter is provided. Updates stay volatile until `save` is issued.
- `save` writes the current runtime configuration to the reserved internal-flash config sector.
- `exit_config` returns `EXIT_CONFIG,STREAMING` and resumes the normal stream.
- `reset` returns `RESET,REBOOTING` and then performs a full MCU reset.

The configuration record is stored with a CRC32 in a reserved 16 KB flash sector at the end of internal flash. On boot, the firmware validates the record before loading it. If the flash region is virgin or the CRC check fails, built-in defaults are used instead.

Error responses are returned as `!,ERROR,<code>,<crc8>`, with coverage for malformed frames, bad CRC, bad sign, unknown commands, and oversized lines.

`TRIGGER` emits `OK:TRIGGER` and then schedules the same UART packet transmission path used by the button-triggered flow.

For the default smoke suite, the startup serial banner is used as the boot synchronization marker. After boot, the firmware streams packet data continuously in normal streaming mode as compact binary COBS frames. The `TEST:AUTO_TRIGGER` marker is reserved for an actual user-button press rather than periodic monitor traffic.

## Normal Stream Protocol

The default machine-to-machine stream is a filtered-only binary packet framed with COBS and terminated by a single zero byte.

Each decoded payload is little-endian `struct <IIHh8H>`:

- `age_us`: packet age in microseconds at transmit time
- `status_flags`: DSP/status flags for that frame
- `filtered_vdda_mv`: filtered VDDA estimate in millivolts
- `filtered_temperature_c`: filtered MCU temperature in degrees Celsius
- `filtered_adc1_mv[4]`: filtered ADC1 rank 1..4 in millivolts
- `filtered_adc2_mv[4]`: filtered ADC2 rank 1..4 in millivolts

That default live-stream payload is `28 bytes` before COBS framing. It replaces the previous human-readable `SEQ/SAMPLE/FILTERED` text packets.

## High-Rate Live Stream Protocol

The high-rate filtered stream is now a live UART stream at `800 Hz` rather than a buffered capture dump.

The host starts the stream with a binary request framed as:

```text
0x00 <COBS-encoded request payload> 0x00
```

The decoded request payload is little-endian `struct <BBI>`:

- `request_type`: `1` for `START_HIGH_RATE_STREAM`
- `protocol_version`: `1`
- `frame_count`: number of live data frames requested

The firmware responds with COBS-framed binary packets, each terminated by a single zero byte.

The begin and end control payloads are little-endian `struct <BBI>`:

- `frame_type`: `1` for begin, `3` for end
- `protocol_version`: `1`
- `frame_count`: requested frame count on begin, transmitted frame count on end

Each live data payload is little-endian `struct <II8H>`:

- `age_us`: packet age in microseconds at transmit time
- `status_flags`: DSP/status flags for that frame
- `adc1_mv[4]`: filtered ADC1 rank 1..4 in millivolts
- `adc2_mv[4]`: filtered ADC2 rank 1..4 in millivolts

That live data payload is `24 bytes` before COBS framing. With one COBS code byte and one zero delimiter byte on the wire, the stream stays below the `250000` baud UART limit at `800 Hz`.

The Robot/Python HIL library uses the binary request path via `Send High Rate Stream Request` and decodes the stream with `Capture High Rate Stream`.

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

The regression level now includes ADC regression checks plus config-mode positive and negative protocol tests.

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
