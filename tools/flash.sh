#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

cd "${PROJECT_DIR}"

echo "=== Verifying firmware safety ==="
python3 tools/verify_firmware.py

BIN_PATH="build/AI-Passport-Baye.bin"
if [ ! -f "${BIN_PATH}" ]; then
    echo "Error: ${BIN_PATH} not found. Run tools/build_docker.sh first."
    exit 1
fi

PORT="${1:-}"
if [ -z "${PORT}" ]; then
    # Auto-detect cu.usbmodem*
    PORTS=($(ls -1 /dev/cu.usbmodem* 2>/dev/null || true))
    if [ ${#PORTS[@]} -eq 0 ]; then
        echo "Error: No /dev/cu.usbmodem* detected. Please connect AI Passport via USB."
        exit 1
    elif [ ${#PORTS[@]} -eq 1 ]; then
        PORT="${PORTS[0]}"
    else
        echo "Multiple ports detected: ${PORTS[*]}"
        echo "Please specify port: ./tools/flash.sh <PORT>"
        exit 1
    fi
fi

echo "Target port: ${PORT}"
echo "Flashing ${BIN_PATH} strictly to partition app (0x10000)..."
echo "NEVER flashing 0x0 or modifying recovery/cardid partitions!"

/Users/gravity/.local/bin/uv run --with esptool python3 -m esptool \
    --chip esp32c3 \
    --port "${PORT}" \
    --baud 921600 \
    --before default_reset \
    --after hard_reset \
    write_flash \
    0x10000 "${BIN_PATH}"

echo "=== Flash complete! ==="
