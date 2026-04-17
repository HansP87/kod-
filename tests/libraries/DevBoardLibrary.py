import os
import re
import subprocess
import time
from pathlib import Path

import serial
from robot.api.deco import keyword, library
from serial import SerialException
from serial.tools import list_ports


HEADER_PATTERN = re.compile(
    r"^SEQ:(?P<sequence>\d+) TS:(?P<timestamp>\d+) READY_US:(?P<ready_us>\d+) "
    r"AGE_US:(?P<age_us>\d+) FLAGS:0x(?P<flags>[0-9A-Fa-f]+)$"
)
SAMPLE_PATTERN = re.compile(
    r"^SAMPLE (?P<index>\d+): VDDA=(?P<vdda_mv>\d+) mV TEMP=(?P<temp_c>-?\d+) C "
    r"ADC1=(?P<adc1>[0-9,]+) ADC2=(?P<adc2>[0-9,]+)$"
)
STARTUP_PATTERN = re.compile(r"^STARTUP:MCU_SERIAL=(?P<serial>[0-9A-F]{24})$")

COMMAND_REQUEST_SIGN = "@"
COMMAND_RESPONSE_SIGN = "!"
COMMAND_CRC8_SEED = 0xA5
COMMAND_CRC8_POLYNOMIAL = 0x07


@library(scope="SUITE")
class DevBoardLibrary:
    def __init__(self):
        self.workspace_dir = Path(__file__).resolve().parents[2]
        self.serial_port = None
        self.last_lines = []

    def _build_env(self):
        env = os.environ.copy()
        env["PATH"] = (
            f"{self.workspace_dir / '.vscode' / 'toolchain-bin'}:"
            f"/Applications/ArmGNUToolchain/15.2.rel1/arm-none-eabi/bin:"
            f"{env.get('PATH', '')}"
        )
        return env

    def _remember_line(self, line):
        self.last_lines.append(line)
        if len(self.last_lines) > 20:
            self.last_lines = self.last_lines[-20:]

    def _read_line_until(self, timeout_seconds):
        if self.serial_port is None:
            raise AssertionError("UART is not open.")

        deadline = time.monotonic() + float(timeout_seconds)
        while time.monotonic() < deadline:
            raw_line = self.serial_port.readline()
            if not raw_line:
                continue

            line = raw_line.decode("utf-8", errors="replace").strip()
            if not line:
                continue

            self._remember_line(line)
            return line

        raise AssertionError(f"Timeout waiting for UART data. Recent lines: {self.last_lines}")

    @keyword
    def build_firmware(self):
        firmware_path = self.workspace_dir / "Debug" / "devADC-dk.elf"

        subprocess.run(
            ["make", "-C", "Debug", "all", "-j4"],
            cwd=self.workspace_dir,
            env=self._build_env(),
            check=True,
        )

        if not firmware_path.is_file():
            raise AssertionError(f"Expected firmware image was not produced: {firmware_path}")

    @keyword
    def flash_firmware(self):
        flash_script = self.workspace_dir / "tests" / "scripts" / "flash_firmware.sh"
        subprocess.run(
            [str(flash_script)],
            cwd=self.workspace_dir,
            env=self._build_env(),
            check=True,
        )
        time.sleep(0.2)

    @keyword
    def auto_detect_serial_port(self, timeout_seconds=10.0):
        requested_port = os.environ.get("DEVADC_SERIAL_PORT")
        if requested_port:
            return requested_port

        deadline = time.monotonic() + float(timeout_seconds)

        while time.monotonic() < deadline:
            ranked_ports = sorted(list_ports.comports(), key=self._serial_port_score, reverse=True)
            if ranked_ports and self._serial_port_score(ranked_ports[0]) > 0:
                return ranked_ports[0].device
            time.sleep(0.2)

        raise AssertionError("No STM32 serial port found. Set DEVADC_SERIAL_PORT explicitly.")

    def _serial_port_score(self, port_info):
        text = " ".join(
            value for value in [
                port_info.device,
                port_info.description,
                port_info.manufacturer,
                port_info.product,
                port_info.interface,
            ] if value
        ).lower()

        score = 0
        if port_info.device.startswith("/dev/cu.usbmodem"):
            score += 50
        if port_info.device.startswith("/dev/cu.usbserial"):
            score += 40
        if "stlink" in text:
            score += 50
        if "stm" in text:
            score += 30
        if "virtual com port" in text:
            score += 30
        return score

    @keyword
    def open_uart(self, port, baudrate=115200, timeout_seconds=0.2, connect_timeout_seconds=10.0):
        self.close_uart()

        deadline = time.monotonic() + float(connect_timeout_seconds)
        last_error = None

        while time.monotonic() < deadline:
            try:
                self.serial_port = serial.Serial(port, int(baudrate), timeout=float(timeout_seconds))
                self.serial_port.reset_input_buffer()
                self.serial_port.reset_output_buffer()
                return
            except SerialException as error:
                last_error = error
                self.serial_port = None
                time.sleep(0.5)

        raise AssertionError(f"Failed to open UART port {port}: {last_error}")

    @keyword
    def close_uart(self):
        if self.serial_port is not None:
            self.serial_port.close()
            self.serial_port = None

    @keyword
    def wait_for_monitor_ready(self, timeout_seconds=10.0):
        return self.wait_for_startup_banner(timeout_seconds)

    @keyword
    def synchronize_with_monitor(self, timeout_seconds=10.0):
        return self.wait_for_startup_banner(timeout_seconds)

    @keyword
    def send_command(self, command):
        if self.serial_port is None:
            raise AssertionError("UART is not open.")

        message = f"{command.strip()}\n".encode("ascii")
        self.serial_port.write(message)
        self.serial_port.flush()

    @keyword
    def wait_for_line_matching(self, pattern, timeout_seconds=5.0):
        matcher = re.compile(pattern)
        deadline = time.monotonic() + float(timeout_seconds)

        while time.monotonic() < deadline:
            line = self._read_line_until(deadline - time.monotonic())
            if matcher.search(line):
                return line

        raise AssertionError(f"Timeout waiting for pattern {pattern!r}. Recent lines: {self.last_lines}")

    @keyword
    def wait_for_no_line_matching(self, pattern, timeout_seconds=3.0):
        matcher = re.compile(pattern)
        deadline = time.monotonic() + float(timeout_seconds)

        while time.monotonic() < deadline:
            try:
                line = self._read_line_until(min(0.25, deadline - time.monotonic()))
            except AssertionError:
                continue

            if matcher.search(line):
                raise AssertionError(f"Unexpected line matching {pattern!r}: {line}")

    @keyword
    def enter_config_mode(self, timeout_seconds=5.0):
        self.send_command("CONFIGMODE")
        return self.wait_for_line_matching(r"^ENTERED CONFIGMODE$", timeout_seconds)

    def _compute_crc8(self, payload):
        crc = COMMAND_CRC8_SEED

        for byte in payload.encode("ascii"):
            crc ^= byte
            for _ in range(8):
                if crc & 0x80:
                    crc = ((crc << 1) ^ COMMAND_CRC8_POLYNOMIAL) & 0xFF
                else:
                    crc = (crc << 1) & 0xFF

        return crc

    @keyword
    def send_config_command(self, command_name, *parameters):
        payload_fields = [COMMAND_REQUEST_SIGN, command_name]
        payload_fields.extend(str(parameter) for parameter in parameters)
        payload = ",".join(payload_fields)
        crc = self._compute_crc8(payload)
        self.send_command(f"{payload},{crc:02X}")

    @keyword
    def send_config_command_with_crc(self, command_name, crc_text, *parameters):
        payload_fields = [COMMAND_REQUEST_SIGN, command_name]
        payload_fields.extend(str(parameter) for parameter in parameters)
        payload = ",".join(payload_fields)
        self.send_command(f"{payload},{crc_text}")

    @keyword
    def send_config_command_with_sign(self, sign, command_name, *parameters):
        payload_fields = [str(sign), command_name]
        payload_fields.extend(str(parameter) for parameter in parameters)
        payload = ",".join(payload_fields)
        crc = self._compute_crc8(payload)
        self.send_command(f"{payload},{crc:02X}")

    def _parse_config_response(self, line):
        fields = line.split(",")
        if len(fields) < 3:
            raise AssertionError(f"Malformed config response: {line}")

        if fields[0] != COMMAND_RESPONSE_SIGN:
            raise AssertionError(f"Unexpected config response sign in line: {line}")

        payload = ",".join(fields[:-1])
        expected_crc = self._compute_crc8(payload)
        try:
            received_crc = int(fields[-1], 16)
        except ValueError as error:
            raise AssertionError(f"Invalid CRC field in line: {line}") from error

        if received_crc != expected_crc:
            raise AssertionError(
                f"CRC mismatch for line {line!r}: expected {expected_crc:02X}, got {received_crc:02X}"
            )

        return {
            "sign": fields[0],
            "command": fields[1],
            "parameters": fields[2:-1],
            "crc": received_crc,
            "line": line,
        }

    @keyword
    def wait_for_config_response(self, expected_command, timeout_seconds=5.0):
        deadline = time.monotonic() + float(timeout_seconds)

        while time.monotonic() < deadline:
            line = self._read_line_until(deadline - time.monotonic())
            if not line.startswith(f"{COMMAND_RESPONSE_SIGN},"):
                continue

            response = self._parse_config_response(line)
            if response["command"] == expected_command:
                return response

        raise AssertionError(
            f"Timeout waiting for config response {expected_command!r}. Recent lines: {self.last_lines}"
        )

    @keyword
    def response_parameter_should_match(self, response, parameter_index, pattern):
        value = response["parameters"][int(parameter_index)]
        if re.fullmatch(pattern, value) is None:
            raise AssertionError(
                f"Response parameter {parameter_index} value {value!r} does not match pattern {pattern!r}"
            )

    @keyword
    def response_parameter_should_equal(self, response, parameter_index, expected_value):
        value = response["parameters"][int(parameter_index)]
        if value != expected_value:
            raise AssertionError(
                f"Response parameter {parameter_index} value {value!r} does not equal {expected_value!r}"
            )

    @keyword
    def wait_for_startup_banner(self, timeout_seconds=5.0):
        line = self.wait_for_line_matching(r"^STARTUP:MCU_SERIAL=[0-9A-F]{24}$", timeout_seconds)
        match = STARTUP_PATTERN.match(line)
        if match is None:
            raise AssertionError(f"Malformed startup banner: {line}")
        return match.group("serial")

    @keyword
    def response_parameter_should_have_count(self, response, expected_count):
        actual_count = len(response["parameters"])
        if actual_count != int(expected_count):
            raise AssertionError(f"Expected {expected_count} parameters, got {actual_count}: {response}")

    @keyword
    def values_should_be_equal(self, actual_value, expected_value):
        if str(actual_value) != str(expected_value):
            raise AssertionError(f"Expected {expected_value!r}, got {actual_value!r}")

    @keyword
    def trigger_sample(self, timeout_seconds=5.0):
        self.send_command("TRIGGER")
        self.wait_for_line_matching(r"^OK:TRIGGER$", timeout_seconds)
        return self.read_tx_packet(timeout_seconds)

    @keyword
    def read_tx_packet(self, timeout_seconds=5.0, sample_count=2):
        header_line = self.wait_for_line_matching(r"^SEQ:\d+", timeout_seconds)
        header_match = HEADER_PATTERN.match(header_line)
        if header_match is None:
            raise AssertionError(f"Malformed packet header: {header_line}")

        packet = {
            key: int(value, 16) if key == "flags" else int(value)
            for key, value in header_match.groupdict().items()
        }
        packet["samples"] = []

        deadline = time.monotonic() + float(timeout_seconds)
        for _ in range(int(sample_count)):
            sample_line = self.wait_for_line_matching(r"^SAMPLE \d+:", max(deadline - time.monotonic(), 0.1))
            sample_match = SAMPLE_PATTERN.match(sample_line)
            if sample_match is None:
                raise AssertionError(f"Malformed sample line: {sample_line}")

            sample = {
                "index": int(sample_match.group("index")),
                "vdda_mv": int(sample_match.group("vdda_mv")),
                "temp_c": int(sample_match.group("temp_c")),
                "adc1": [int(value) for value in sample_match.group("adc1").split(",")],
                "adc2": [int(value) for value in sample_match.group("adc2").split(",")],
            }
            packet["samples"].append(sample)

        return packet

    @keyword
    def packet_field_should_be_within_range(self, packet, field_name, minimum, maximum):
        value = int(packet[field_name])
        lower_bound = int(minimum)
        upper_bound = int(maximum)
        if not (lower_bound <= value <= upper_bound):
            raise AssertionError(
                f"Packet field {field_name} out of range: {value} not in [{lower_bound}, {upper_bound}]"
            )

    @keyword
    def sample_field_should_be_within_range(self, packet, sample_index, field_name, minimum, maximum):
        sample = packet["samples"][int(sample_index)]
        value = int(sample[field_name])
        lower_bound = int(minimum)
        upper_bound = int(maximum)
        if not (lower_bound <= value <= upper_bound):
            raise AssertionError(
                f"Sample {sample_index} field {field_name} out of range: {value} not in [{lower_bound}, {upper_bound}]"
            )

    @keyword
    def sample_channel_should_be_within_range(self, packet, sample_index, field_name, channel_index, minimum, maximum):
        sample = packet["samples"][int(sample_index)]
        channel_values = sample[field_name]
        value = int(channel_values[int(channel_index)])
        lower_bound = int(minimum)
        upper_bound = int(maximum)
        if not (lower_bound <= value <= upper_bound):
            raise AssertionError(
                f"Sample {sample_index} field {field_name}[{channel_index}] out of range: "
                f"{value} not in [{lower_bound}, {upper_bound}]"
            )

    @keyword
    def packet_field_should_equal(self, packet, field_name, expected_value):
        value = int(packet[field_name])
        expected = int(expected_value)
        if value != expected:
            raise AssertionError(f"Packet field {field_name} expected {expected}, got {value}")
