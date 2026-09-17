#!/usr/bin/env python3
"""Automated OST hardware verification suite for AI-Passport-Baye."""

from __future__ import annotations

import sys
import time
import serial


def send_and_wait(ser: serial.Serial, char: str, wait_sec: float = 1.0, log_prefix: str = "") -> str:
    print(f"\n>>> [{log_prefix}] Sending: {repr(char)}")
    ser.write(char.encode("utf-8"))
    ser.flush()
    deadline = time.time() + wait_sec
    captured = []
    while time.time() < deadline:
        line = ser.readline()
        if line:
            text = line.decode("utf-8", errors="replace")
            sys.stdout.write(text)
            sys.stdout.flush()
            captured.append(text)
    return "".join(captured)


def main() -> int:
    port = sys.argv[1] if len(sys.argv) > 1 else "/dev/cu.usbmodem1101"
    print(f"Opening {port}...")
    ser = serial.Serial(port, 115200, timeout=0.1)
    ser.dtr = False
    ser.rts = False

    with ser:
        ser.reset_input_buffer()
        print("Connected. Listening for initial output...")
        time.sleep(1.0)
        while ser.in_waiting:
            sys.stdout.write(ser.read(ser.in_waiting).decode("utf-8", errors="replace"))
            sys.stdout.flush()

        # Step 1: Telemetry dump
        out = send_and_wait(ser, "m", 1.5, "TELEMETRY-DUMP")

        # Step 2: Direct Audition Test of all 5 tracks via serial shortcuts
        print("\n================== STEP 2: AUDITION ALL 5 TRACKS ==================")
        # Track 2: STRATEGY
        out = send_and_wait(ser, "2", 2.5, "TRIGGER-STRATEGY")
        assert "Switching to track [STRATEGY]" in out or "STRATEGY" in out, "Failed to switch to STRATEGY"

        # Track 3: BATTLE
        out = send_and_wait(ser, "3", 2.5, "TRIGGER-BATTLE")
        assert "Switching to track [BATTLE]" in out or "BATTLE" in out, "Failed to switch to BATTLE"

        # Track 4: VICTORY one-shot (6s) -> auto resume to STRATEGY
        print("\n--- Auditioning VICTORY (one-shot ~6s) ---")
        out = send_and_wait(ser, "4", 7.5, "TRIGGER-VICTORY-RESUME")
        assert "Switching to track [VICTORY]" in out or "VICTORY" in out, "Failed to switch to VICTORY"
        assert "auto-resuming [STRATEGY]" in out or "Switching to track [STRATEGY]" in out, "Failed to auto-resume STRATEGY"

        # Track 5: DEFEAT one-shot (9s) -> auto resume to STRATEGY
        print("\n--- Auditioning DEFEAT (one-shot ~9s) ---")
        out = send_and_wait(ser, "5", 10.5, "TRIGGER-DEFEAT-RESUME")
        assert "Switching to track [DEFEAT]" in out or "DEFEAT" in out, "Failed to switch to DEFEAT"
        assert "auto-resuming [STRATEGY]" in out or "Switching to track [STRATEGY]" in out, "Failed to auto-resume STRATEGY"

        # Track 1: Back to TITLE
        out = send_and_wait(ser, "1", 2.5, "TRIGGER-TITLE")
        assert "Switching to track [TITLE]" in out or "TITLE" in out, "Failed to switch to TITLE"

        # Step 3: Volume adjustment & persistence test
        print("\n================== STEP 3: VOLUME ADJUSTMENT ==================")
        out = send_and_wait(ser, "]", 1.5, "VOL-UP-80")
        assert "Persisted volume 80%" in out or "VOL 80" in out, "Failed to adjust volume up"

        out = send_and_wait(ser, "[", 1.5, "VOL-DOWN-70")
        assert "Persisted volume 70%" in out or "VOL 70" in out, "Failed to adjust volume down"

        # Step 4: Game Menu to Strategy transition test
        print("\n================== STEP 4: GAMEPLAY SCENE TRANSITION ==================")
        # In Main Menu: press Enter to select "新君登基"
        send_and_wait(ser, "\r", 1.5, "MENU-SELECT-NEW-GAME")
        # In Period Menu: press Enter to select period
        send_and_wait(ser, "\r", 1.5, "SELECT-PERIOD")
        # In King Menu: press Enter to select king
        out = send_and_wait(ser, "\r", 3.0, "SELECT-KING-ENTER-GAME")
        assert "Entering GameDevDrv()" in out, "Failed to enter GameDevDrv"
        assert "Switching to track [STRATEGY]" in out, "Failed to auto-switch to STRATEGY upon entering game"

        # Step 5: Final Telemetry dump
        print("\n================== STEP 5: FINAL TELEMETRY DUMP ==================")
        out = send_and_wait(ser, "m", 2.0, "FINAL-TELEMETRY")
        assert "Underruns:       0" in out, "Detected audio underruns!"
        assert "DMA Timeouts:   0" in out, "Detected display DMA timeouts!"
        assert "LCD Sub Fails:  0" in out, "Detected display submit failures!"

        print("\n=======================================================")
        print("ALL HARDWARE OST VERIFICATIONS PASSED SUCCESSFULLY!")
        print("=======================================================")
    return 0


if __name__ == "__main__":
    sys.exit(main())
