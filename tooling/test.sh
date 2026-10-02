#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
source "$root/tooling/versions.env"
if [[ ! -f /run/.containerenv ]]; then
    exec distrobox enter --no-tty --clean-path --name "$DISTROBOX_NAME" -- \
        bash "$root/tooling/test.sh" "$@"
fi

mkdir -p "$root/build/tests"
clang-format --dry-run --Werror "$root"/firmware/*.c "$root"/firmware/*.h \
    "$root/tests/protocol_harness.c"
cc -std=c11 -shared -fPIC -O2 -DLZ4_FORCE_MEMORY_ACCESS=0 \
    -I "$root/third_party/gud" -I "$root/firmware" \
    "$root/tests/protocol_harness.c" "$root/third_party/gud/gud.c" \
    "$root/third_party/gud/lz4.c" "$root/firmware/convert.c" \
    -o "$root/build/tests/libprotocol.so"
python3 -m pytest -q "$root/tests" "$@"

cc -std=c11 -g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer \
    -I "$root/third_party/gud" \
    "$root/tests/protocol_harness.c" "$root/third_party/gud/gud.c" \
    -o "$root/build/tests/protocol-sanitized"
ASAN_OPTIONS=detect_leaks=1 "$root/build/tests/protocol-sanitized"
echo "Protocol sanitizer checks passed."
