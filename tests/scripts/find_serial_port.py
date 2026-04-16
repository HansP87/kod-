#!/usr/bin/env python3
import os
import sys

from serial.tools import list_ports


def port_score(port_info):
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


def main():
    requested_port = os.environ.get("DEVADC_SERIAL_PORT")
    if requested_port:
        print(requested_port)
        return 0

    ports = list(list_ports.comports())
    if not ports:
        print("No serial ports detected.", file=sys.stderr)
        return 1

    ranked_ports = sorted(ports, key=port_score, reverse=True)
    if port_score(ranked_ports[0]) <= 0:
        print("No STM32-like serial port detected. Set DEVADC_SERIAL_PORT explicitly.", file=sys.stderr)
        return 1

    print(ranked_ports[0].device)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
