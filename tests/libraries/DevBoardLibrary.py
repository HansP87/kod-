import os
import re
import statistics
import struct
import subprocess
import time
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
import serial
from robot.api import logger
from robot.api.deco import keyword, library
from robot.libraries.BuiltIn import BuiltIn
from robot.libraries.BuiltIn import RobotNotRunningError
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
FILTERED_PATTERN = re.compile(
    r"^FILTERED (?P<index>\d+): VDDA=(?P<vdda_mv>\d+) mV TEMP=(?P<temp_c>-?\d+) C "
    r"ADC1=(?P<adc1>[0-9,]+) ADC2=(?P<adc2>[0-9,]+)$"
)
STARTUP_PATTERN = re.compile(r"^STARTUP:MCU_SERIAL=(?P<serial>[0-9A-F]{24})$")

RAW_SAMPLE_COUNT = 4
FILTERED_SAMPLE_COUNT = 1
HIGH_RATE_CAPTURE_PROTOCOL_VERSION = 1
HIGH_RATE_REQUEST_TYPE_START = 1
HIGH_RATE_FRAME_TYPE_BEGIN = 1
HIGH_RATE_FRAME_TYPE_END = 3
HIGH_RATE_SAMPLE_RATE_HZ = 800.0
NORMAL_STREAM_STRUCT = struct.Struct("<IIHh8H")
HIGH_RATE_CAPTURE_STRUCT = struct.Struct("<II8H")
HIGH_RATE_CAPTURE_CONTROL_STRUCT = struct.Struct("<BBI")
HIGH_RATE_CAPTURE_REQUEST_STRUCT = struct.Struct("<BBI")

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

    def _read_cobs_frame_until(self, timeout_seconds):
        if self.serial_port is None:
            raise AssertionError("UART is not open.")

        deadline = time.monotonic() + float(timeout_seconds)
        encoded = bytearray()
        while time.monotonic() < deadline:
            raw_byte = self.serial_port.read(1)
            if not raw_byte:
                continue

            if raw_byte == b"\x00":
                if not encoded:
                    continue
                return self._decode_cobs(bytes(encoded))

            encoded.extend(raw_byte)

        raise AssertionError("Timeout waiting for COBS frame.")

    def _read_valid_cobs_frame_until(self, timeout_seconds, expected_length):
        deadline = time.monotonic() + float(timeout_seconds)
        last_error = None

        while time.monotonic() < deadline:
            try:
                payload = self._read_cobs_frame_until(max(deadline - time.monotonic(), 0.05))
            except AssertionError as error:
                last_error = error
                if "Timeout waiting for COBS frame" in str(error):
                    break
                continue

            if len(payload) == int(expected_length):
                return payload

            last_error = AssertionError(
                f"Unexpected COBS payload size: expected {expected_length}, got {len(payload)}"
            )

        if last_error is not None:
            raise AssertionError(str(last_error))

        raise AssertionError("Timeout waiting for COBS frame.")

    def _decode_cobs(self, encoded):
        decoded = bytearray()
        index = 0
        encoded_length = len(encoded)

        while index < encoded_length:
            code = encoded[index]
            if code == 0:
                raise AssertionError("Invalid COBS frame: zero byte inside encoded payload")
            index += 1

            next_index = index + code - 1
            if next_index > encoded_length:
                raise AssertionError("Invalid COBS frame: code exceeds payload length")

            decoded.extend(encoded[index:next_index])
            index = next_index

            if code != 0xFF and index < encoded_length:
                decoded.append(0)

        return bytes(decoded)

    def _encode_cobs(self, payload):
        encoded = bytearray([0])
        code_index = 0
        code = 1

        for byte in payload:
            if byte == 0:
                encoded[code_index] = code
                code_index = len(encoded)
                encoded.append(0)
                code = 1
            else:
                encoded.append(byte)
                code += 1
                if code == 0xFF:
                    encoded[code_index] = code
                    code_index = len(encoded)
                    encoded.append(0)
                    code = 1

        encoded[code_index] = code
        return bytes(encoded)

    def _resolve_robot_output_dir(self):
        try:
            return Path(BuiltIn().get_variable_value("${OUTPUT DIR}"))
        except RobotNotRunningError:
            return self.workspace_dir / "tests" / "results" / "regression"

    def _log_embedded_image(self, output_path, alt_text):
        output_dir = self._resolve_robot_output_dir()

        try:
            relative_path = output_path.relative_to(output_dir).as_posix()
            logger.info(
                (
                    f'<a href="{relative_path}">'
                    f'<img src="{relative_path}" alt="{alt_text}" '
                    f'style="max-width: 100%; height: auto; border: 1px solid #ccc;" />'
                    f"</a>"
                ),
                html=True,
            )
        except ValueError:
            logger.info(f"Plot written to {output_path}")

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
    def open_uart(self, port, baudrate=250000, timeout_seconds=0.2, connect_timeout_seconds=10.0):
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
    def send_high_rate_stream_request(self, frame_count):
        if self.serial_port is None:
            raise AssertionError("UART is not open.")

        payload = HIGH_RATE_CAPTURE_REQUEST_STRUCT.pack(
            HIGH_RATE_REQUEST_TYPE_START,
            HIGH_RATE_CAPTURE_PROTOCOL_VERSION,
            int(frame_count),
        )
        framed = b"\x00" + self._encode_cobs(payload) + b"\x00"
        self.serial_port.write(framed)
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
    def read_tx_packet(self, timeout_seconds=5.0, sample_count=RAW_SAMPLE_COUNT, filtered_count=FILTERED_SAMPLE_COUNT):
        del sample_count
        del filtered_count

        payload = self._read_valid_cobs_frame_until(timeout_seconds, NORMAL_STREAM_STRUCT.size)
        unpacked = NORMAL_STREAM_STRUCT.unpack(payload)

        filtered_sample = {
            "index": 0,
            "vdda_mv": int(unpacked[2]),
            "temp_c": int(unpacked[3]),
            "adc1": [int(value) for value in unpacked[4:8]],
            "adc2": [int(value) for value in unpacked[8:12]],
        }

        return {
            "age_us": int(unpacked[0]),
            "flags": int(unpacked[1]),
            "samples": [],
            "filtered": [filtered_sample],
        }

    @keyword
    def wait_for_no_stream_packet(self, timeout_seconds=3.0):
        try:
            packet = self.read_tx_packet(timeout_seconds)
        except AssertionError:
            return

        raise AssertionError(f"Unexpected stream packet received: {packet}")

    @keyword
    def capture_high_rate_stream(self, frame_count, timeout_seconds=20.0):
        expected_count = int(frame_count)
        if self.serial_port is not None:
            self.serial_port.reset_input_buffer()
        self.send_high_rate_stream_request(expected_count)

        deadline = time.monotonic() + float(timeout_seconds)
        begin_payload = self._read_valid_cobs_frame_until(max(deadline - time.monotonic(), 0.1), HIGH_RATE_CAPTURE_CONTROL_STRUCT.size)
        if len(begin_payload) != HIGH_RATE_CAPTURE_CONTROL_STRUCT.size:
            raise AssertionError(f"Unexpected high-rate begin payload size: {len(begin_payload)}")

        begin_frame_type, begin_protocol_version, begin_count = HIGH_RATE_CAPTURE_CONTROL_STRUCT.unpack(begin_payload)
        if begin_frame_type != HIGH_RATE_FRAME_TYPE_BEGIN:
            raise AssertionError(f"Expected high-rate begin frame, got type {begin_frame_type}")
        if begin_protocol_version != HIGH_RATE_CAPTURE_PROTOCOL_VERSION:
            raise AssertionError(f"Unsupported high-rate protocol version: {begin_protocol_version}")
        if begin_count != expected_count:
            raise AssertionError(f"High-rate begin count {begin_count} != requested count {expected_count}")

        frames = []
        for _ in range(expected_count):
            payload = self._read_valid_cobs_frame_until(max(deadline - time.monotonic(), 0.1), HIGH_RATE_CAPTURE_STRUCT.size)
            if len(payload) != HIGH_RATE_CAPTURE_STRUCT.size:
                raise AssertionError(f"Unexpected high-rate payload size: {len(payload)}")

            unpacked = HIGH_RATE_CAPTURE_STRUCT.unpack(payload)

            frames.append(
                {
                    "age_us": unpacked[0],
                    "flags": unpacked[1],
                    "adc1": list(unpacked[2:6]),
                    "adc2": list(unpacked[6:10]),
                }
            )

        end_payload = self._read_valid_cobs_frame_until(max(deadline - time.monotonic(), 0.1), HIGH_RATE_CAPTURE_CONTROL_STRUCT.size)
        if len(end_payload) != HIGH_RATE_CAPTURE_CONTROL_STRUCT.size:
            raise AssertionError(f"Unexpected high-rate end payload size: {len(end_payload)}")

        end_frame_type, end_protocol_version, end_count = HIGH_RATE_CAPTURE_CONTROL_STRUCT.unpack(end_payload)
        if end_frame_type != HIGH_RATE_FRAME_TYPE_END:
            raise AssertionError(f"Expected high-rate end frame, got type {end_frame_type}")
        if end_protocol_version != HIGH_RATE_CAPTURE_PROTOCOL_VERSION:
            raise AssertionError(f"Unsupported high-rate protocol version: {end_protocol_version}")
        if end_count != expected_count:
            raise AssertionError(f"High-rate end count {end_count} != requested count {expected_count}")

        return frames

    @keyword
    def high_rate_channel_should_be_stable(self, frames, adc_name, channel_index, maximum_stdev):
        values = [int(frame[adc_name][int(channel_index)]) for frame in frames]
        signal_stdev = statistics.pstdev(values)
        if signal_stdev > float(maximum_stdev):
            raise AssertionError(
                f"Expected {adc_name}[{channel_index}] to stay within std-dev {maximum_stdev}, got {signal_stdev:.3f}"
            )
        return signal_stdev

    @keyword
    def high_rate_flags_should_equal(self, frames, expected_value):
        expected = int(expected_value)
        for index, frame in enumerate(frames):
            if int(frame["flags"]) != expected:
                raise AssertionError(f"Frame {index} flags {frame['flags']} != {expected}")

    @keyword
    def plot_high_rate_filtered_channels(self, frames, output_name="high_rate_filtered_channels.png"):
        output_dir = self._resolve_robot_output_dir()
        output_path = output_dir / output_name
        output_path.parent.mkdir(parents=True, exist_ok=True)

        x_axis = list(range(len(frames)))
        adc1_channels = [[int(frame["adc1"][channel]) for frame in frames] for channel in range(4)]
        adc2_channels = [[int(frame["adc2"][channel]) for frame in frames] for channel in range(4)]
        age_values = [int(frame["age_us"]) for frame in frames]

        figure, axes = plt.subplots(2, 1, figsize=(12, 8), sharex=True)
        for channel, values in enumerate(adc1_channels, start=1):
            axes[0].plot(x_axis, values, linewidth=1.4, label=f"ADC1 rank {channel}")
        axes[0].set_title("Filtered ADC1 channels over capture")
        axes[0].set_ylabel("mV")
        axes[0].grid(True, alpha=0.25)
        axes[0].legend(loc="upper right", ncol=2)

        for channel, values in enumerate(adc2_channels, start=1):
            axes[1].plot(x_axis, values, linewidth=1.4, label=f"ADC2 rank {channel}")
        axes[1].set_xlabel("Frame index")
        axes[1].set_ylabel("mV")
        axes[1].grid(True, alpha=0.25)
        axes[1].legend(loc="upper right", ncol=2)

        figure.suptitle(
            f"COBS high-rate capture: age range {min(age_values)} us to {max(age_values)} us",
            fontsize=12,
        )
        figure.tight_layout()
        figure.savefig(output_path, dpi=150)
        plt.close(figure)

        self._log_embedded_image(output_path, "High-rate filtered capture plot")

        return str(output_path)

    @keyword
    def plot_high_rate_age_jitter(self, frames, output_name="high_rate_age_jitter.png"):
        output_dir = self._resolve_robot_output_dir()
        output_path = output_dir / output_name
        output_path.parent.mkdir(parents=True, exist_ok=True)

        x_axis = list(range(len(frames)))
        age_values = [int(frame["age_us"]) for frame in frames]
        min_age = min(age_values)
        max_age = max(age_values)
        mean_age = statistics.fmean(age_values)
        centered_age = [value - mean_age for value in age_values]

        figure, axes = plt.subplots(2, 1, figsize=(12, 8), sharex=True)
        axes[0].plot(x_axis, age_values, linewidth=1.2, color="#1f77b4")
        axes[0].set_title("High-rate frame age over capture")
        axes[0].set_ylabel("age_us")
        axes[0].grid(True, alpha=0.25)
        axes[0].axhline(mean_age, color="#d62728", linestyle="--", linewidth=1.0, label=f"mean={mean_age:.1f} us")
        axes[0].legend(loc="upper right")

        axes[1].plot(x_axis, centered_age, linewidth=1.2, color="#2ca02c")
        axes[1].set_title("High-rate frame age jitter around mean")
        axes[1].set_xlabel("Frame index")
        axes[1].set_ylabel("jitter_us")
        axes[1].grid(True, alpha=0.25)
        axes[1].axhline(0.0, color="#333333", linestyle="--", linewidth=1.0)

        figure.suptitle(
            f"Age range {min_age} us to {max_age} us, peak-to-peak jitter {max_age - min_age} us",
            fontsize=12,
        )
        figure.tight_layout()
        figure.savefig(output_path, dpi=150)
        plt.close(figure)

        self._log_embedded_image(output_path, "High-rate age jitter plot")

        return str(output_path)

    @keyword
    def plot_high_rate_channel_fft(self, frames, output_name="high_rate_channel_fft.png"):
        output_dir = self._resolve_robot_output_dir()
        output_path = output_dir / output_name
        output_path.parent.mkdir(parents=True, exist_ok=True)

        frame_count = len(frames)
        if frame_count < 2:
            raise AssertionError("Need at least two frames to compute FFT plot.")

        sample_period_s = 1.0 / HIGH_RATE_SAMPLE_RATE_HZ
        frequencies = np.fft.rfftfreq(frame_count, d=sample_period_s)
        window = np.hanning(frame_count)

        figure, axes = plt.subplots(2, 1, figsize=(12, 8), sharex=True)

        for channel in range(4):
            adc1_values = np.array([int(frame["adc1"][channel]) for frame in frames], dtype=float)
            adc1_values = adc1_values - np.mean(adc1_values)
            adc1_spectrum = np.fft.rfft(adc1_values * window)
            adc1_magnitude = np.abs(adc1_spectrum)
            axes[0].plot(frequencies, adc1_magnitude, linewidth=1.2, label=f"ADC1 rank {channel + 1}")

            adc2_values = np.array([int(frame["adc2"][channel]) for frame in frames], dtype=float)
            adc2_values = adc2_values - np.mean(adc2_values)
            adc2_spectrum = np.fft.rfft(adc2_values * window)
            adc2_magnitude = np.abs(adc2_spectrum)
            axes[1].plot(frequencies, adc2_magnitude, linewidth=1.2, label=f"ADC2 rank {channel + 1}")

        axes[0].set_title("ADC1 filtered channel FFT magnitude")
        axes[0].set_ylabel("Magnitude")
        axes[0].grid(True, alpha=0.25)
        axes[0].legend(loc="upper right", ncol=2)

        axes[1].set_title("ADC2 filtered channel FFT magnitude")
        axes[1].set_xlabel("Frequency (Hz)")
        axes[1].set_ylabel("Magnitude")
        axes[1].grid(True, alpha=0.25)
        axes[1].legend(loc="upper right", ncol=2)

        figure.suptitle(
            f"FFT of {frame_count} captured frames at {HIGH_RATE_SAMPLE_RATE_HZ:.0f} Hz (mean removed, Hann window)",
            fontsize=12,
        )
        figure.tight_layout()
        figure.savefig(output_path, dpi=150)
        plt.close(figure)

        self._log_embedded_image(output_path, "High-rate ADC FFT plot")

        return str(output_path)

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
    def filtered_field_should_be_within_range(self, packet, sample_index, field_name, minimum, maximum):
        sample = packet["filtered"][int(sample_index)]
        value = int(sample[field_name])
        lower_bound = int(minimum)
        upper_bound = int(maximum)
        if not (lower_bound <= value <= upper_bound):
            raise AssertionError(
                f"Filtered sample {sample_index} field {field_name} out of range: "
                f"{value} not in [{lower_bound}, {upper_bound}]"
            )

    @keyword
    def filtered_channel_should_be_within_range(self, packet, sample_index, field_name, channel_index, minimum, maximum):
        sample = packet["filtered"][int(sample_index)]
        channel_values = sample[field_name]
        value = int(channel_values[int(channel_index)])
        lower_bound = int(minimum)
        upper_bound = int(maximum)
        if not (lower_bound <= value <= upper_bound):
            raise AssertionError(
                f"Filtered sample {sample_index} field {field_name}[{channel_index}] out of range: "
                f"{value} not in [{lower_bound}, {upper_bound}]"
            )

    @keyword
    def packet_field_should_equal(self, packet, field_name, expected_value):
        value = int(packet[field_name])
        expected = int(expected_value)
        if value != expected:
            raise AssertionError(f"Packet field {field_name} expected {expected}, got {value}")
