#!/usr/bin/env python3
"""Reset an ESP serial port and capture a bounded UTF-8 log sample."""

from __future__ import annotations

import argparse
import sys
import time

import serial


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("port")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--seconds", type=float, default=12.0)
    args = parser.parse_args()

    with serial.Serial(args.port, args.baud, timeout=0.15) as device:
        device.dtr = False
        device.rts = True
        time.sleep(0.12)
        device.rts = False
        time.sleep(0.12)
        device.reset_input_buffer()
        deadline = time.monotonic() + args.seconds
        while time.monotonic() < deadline:
            data = device.read(device.in_waiting or 1)
            if data:
                sys.stdout.write(data.decode("utf-8", errors="replace"))
                sys.stdout.flush()


if __name__ == "__main__":
    main()
