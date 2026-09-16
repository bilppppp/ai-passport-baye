#!/usr/bin/env python3
"""Interactive test and log monitor for Baye on AI Passport."""

from __future__ import annotations

import argparse
import sys
import time
import serial


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("port", default="/dev/cu.usbmodem101", nargs="?")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--keys", type=str, default="", help="Keystroke sequence to send")
    parser.add_argument("--delay", type=float, default=1.0, help="Delay between keys")
    parser.add_argument("--listen", type=float, default=5.0, help="Listen seconds after keys")
    args = parser.parse_args()

    with serial.Serial(args.port, args.baud, timeout=0.1) as ser:
        print(f"Connected to {args.port}")
        ser.reset_input_buffer()

        # Listen initial
        t0 = time.time()
        while time.time() - t0 < 1.0:
            line = ser.readline()
            if line:
                sys.stdout.write(line.decode("utf-8", errors="replace"))
                sys.stdout.flush()

        if args.keys:
            for ch in args.keys:
                print(f"\n>>> Sending key: {repr(ch)}")
                if ch == 'E':  # Represent enter
                    ser.write(b"\r")
                elif ch == 'X':  # Represent exit
                    ser.write(b"\x1b")
                else:
                    ser.write(ch.encode("utf-8"))
                ser.flush()
                time.sleep(args.delay)

                # Read responses
                t_key = time.time()
                while time.time() - t_key < args.delay:
                    line = ser.readline()
                    if line:
                        sys.stdout.write(line.decode("utf-8", errors="replace"))
                        sys.stdout.flush()

        # Tail listen
        print("\n>>> Listening for further output...")
        t_end = time.time() + args.listen
        while time.time() < t_end:
            line = ser.readline()
            if line:
                sys.stdout.write(line.decode("utf-8", errors="replace"))
                sys.stdout.flush()


if __name__ == "__main__":
    main()
