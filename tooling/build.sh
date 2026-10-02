#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
source "$root/tooling/versions.env"

if [[ ! -f /run/.containerenv ]]; then
    exec distrobox enter --no-tty --clean-path --name "$DISTROBOX_NAME" -- \
        bash "$root/tooling/build.sh" "$@"
fi

cmake -S "$root" -B "$root/build/firmware" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DPICO_BOARD=pico_w \
    -DPICO_SDK_PATH="$root/.deps/pico-sdk" \
    -Dpicotool_DIR="$root/.deps/picotool-install/lib/cmake/picotool" \
    "$@"
cmake --build "$root/build/firmware" --parallel 2
mkdir -p "$root/dist"
cp "$root/build/firmware/pico_w_screen.uf2" "$root/dist/pico_w_screen.uf2"
cp "$root/build/firmware/pico_w_screen.elf" "$root/dist/pico_w_screen.elf"
python3 "$root/tooling/check_uf2.py" "$root/dist/pico_w_screen.uf2"
arm-none-eabi-size "$root/dist/pico_w_screen.elf"
sha256sum "$root/dist/pico_w_screen.uf2"
