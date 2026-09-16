#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
user_spec="$(id -u):$(id -g)"

echo "=== Building AI-Passport-Baye with ESP-IDF 5.5.3 ==="

docker run --rm \
  --user "$user_spec" \
  -e HOME=/tmp/sd-idf-home \
  -e CCACHE_DISABLE=1 \
  -v "$project_dir:/project" \
  -w /project \
  espressif/idf:v5.5.3 \
  bash -lc 'SDKCONFIG_DEFAULTS=/project/sdkconfig.defaults idf.py -B /project/build -D SDKCONFIG=/project/build/sdkconfig reconfigure && idf.py -B /project/build build && sync && python3 /project/tools/verify_firmware.py /project/build'
