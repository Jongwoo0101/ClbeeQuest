#!/bin/bash
# TUK-Shell 빌드 및 실행 스크립트 [우진]
# 사용법: ./src/bash/run.sh

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BIN="$SCRIPT_DIR/tuk_shell"

echo "[build] gcc -Wall -Wextra -o tuk_shell src/bash/*.c"
gcc -Wall -Wextra -o "$BIN" "$SCRIPT_DIR"/*.c

echo "[run] $BIN"
exec "$BIN"
