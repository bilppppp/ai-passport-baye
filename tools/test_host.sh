#!/usr/bin/env bash
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="${TMPDIR:-/tmp}/ai-passport-baye-host-tests"
mkdir -p "$BUILD_DIR"

echo "=== Running Host Tests for AI-Passport-Baye ==="

# 1. Test Framebuffer
clang -std=c11 -Wall -Wextra -Werror \
  -I"$PROJECT_DIR/components/baye/platform" \
  -I"$PROJECT_DIR/components/baye/core" \
  -I"$PROJECT_DIR/components/baye/include" \
  "$PROJECT_DIR/components/baye/platform/passport_display.c" \
  "$PROJECT_DIR/tests/test_framebuffer.c" \
  -o "$BUILD_DIR/test_framebuffer"
"$BUILD_DIR/test_framebuffer"

# 2. Test Keys
clang -std=c11 -Wall -Wextra -Werror \
  -I"$PROJECT_DIR/components/baye/platform" \
  -I"$PROJECT_DIR/components/baye/core" \
  -I"$PROJECT_DIR/components/baye/core/inc" \
  -I"$PROJECT_DIR/components/baye/include" \
  -I"$PROJECT_DIR/components/bsp/include" \
  "$PROJECT_DIR/components/baye/platform/passport_display.c" \
  "$PROJECT_DIR/components/baye/platform/passport_gui.c" \
  "$PROJECT_DIR/components/baye/platform/passport_input.c" \
  "$PROJECT_DIR/components/baye/platform/passport_audio.c" \
  "$PROJECT_DIR/tests/test_keys.c" \
  -o "$BUILD_DIR/test_keys"
"$BUILD_DIR/test_keys"

# 3. Test Filesystem & Save Serialization
clang -std=c11 -Wall -Wextra -Werror \
  -I"$PROJECT_DIR/components/baye/platform" \
  -I"$PROJECT_DIR/components/baye/core" \
  -I"$PROJECT_DIR/components/baye/include" \
  "$PROJECT_DIR/components/baye/platform/passport_fsys.c" \
  "$PROJECT_DIR/tests/test_fsys.c" \
  -o "$BUILD_DIR/test_fsys"
"$BUILD_DIR/test_fsys" "$PROJECT_DIR/components/baye/assets/dat.lib" "$PROJECT_DIR/components/baye/assets/font.bin"

# 4. Test Battery Display
clang -std=c11 -Wall -Wextra -Werror \
  -I"$PROJECT_DIR/components/baye/platform" \
  -I"$PROJECT_DIR/components/baye/include" \
  "$PROJECT_DIR/components/baye/platform/passport_battery.c" \
  "$PROJECT_DIR/tests/test_battery.c" \
  -o "$BUILD_DIR/test_battery"
"$BUILD_DIR/test_battery"

# 5. Test Audio Decoding & Streaming Logic
clang -std=c11 -Wall -Wextra -Werror \
  -I"$PROJECT_DIR/components/baye/platform" \
  "$PROJECT_DIR/components/baye/platform/passport_adpcm.c" \
  "$PROJECT_DIR/components/baye/platform/passport_audio.c" \
  "$PROJECT_DIR/tests/test_audio.c" \
  -o "$BUILD_DIR/test_audio"
"$BUILD_DIR/test_audio" "$PROJECT_DIR/components/baye/assets"

echo "ALL HOST TESTS PASSED!"
