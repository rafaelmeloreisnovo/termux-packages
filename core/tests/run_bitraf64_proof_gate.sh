#!/bin/sh
set -eu

cd "$(dirname "$0")/.."

CC_BIN="${CC:-cc}"
OUT="${TMPDIR:-/tmp}/test-bitraf64-proof-gate.$$"
trap 'rm -f "$OUT"' EXIT HUP INT TERM

"$CC_BIN" -std=c11 -O2 -Wall -Wextra -Werror \
  bitraf64_integration.c tests/test_bitraf64_integration.c \
  -lm -o "$OUT"

"$OUT"
