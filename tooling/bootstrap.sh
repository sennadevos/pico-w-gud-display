#!/usr/bin/env bash
set -euo pipefail

if [[ ! -f /run/.containerenv ]]; then
    echo "Run this through tooling/setup.sh; dependencies belong in the distrobox." >&2
    exit 1
fi

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
source "$root/tooling/versions.env"
mkdir -p "$root/.deps"

sdk="$root/.deps/pico-sdk"
if [[ ! -d "$sdk" ]]; then
    git clone --depth 1 --branch "$PICO_SDK_VERSION" \
        https://github.com/raspberrypi/pico-sdk.git "$sdk"
fi
if [[ $(git -C "$sdk" describe --tags --exact-match) != "$PICO_SDK_VERSION" ]]; then
    echo "Unexpected Pico SDK revision in $sdk; refusing to overwrite it." >&2
    exit 1
fi
git -C "$sdk" submodule update --init --depth 1 lib/tinyusb

upstream="$root/.deps/gud-pico"
if [[ ! -d "$upstream" ]]; then
    git clone https://github.com/notro/gud-pico.git "$upstream"
    git -C "$upstream" switch --detach "$GUD_PICO_REV"
fi
if [[ $(git -C "$upstream" rev-parse HEAD) != "$GUD_PICO_REV" ]]; then
    echo "Unexpected GUD reference revision in $upstream; refusing to overwrite it." >&2
    exit 1
fi

picotool="$root/.deps/picotool"
if [[ ! -d "$picotool" ]]; then
    git clone --depth 1 --branch "$PICOTOOL_VERSION" \
        https://github.com/raspberrypi/picotool.git "$picotool"
fi
if [[ $(git -C "$picotool" describe --tags --exact-match) != "$PICOTOOL_VERSION" ]]; then
    echo "Unexpected picotool revision in $picotool; refusing to overwrite it." >&2
    exit 1
fi
cmake -S "$picotool" -B "$root/build/picotool" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DPICO_SDK_PATH="$sdk" \
    -DCMAKE_INSTALL_PREFIX="$root/.deps/picotool-install"
cmake --build "$root/build/picotool" --parallel 2
cmake --install "$root/build/picotool"

echo "Build environment ready in $DISTROBOX_NAME."
arm-none-eabi-gcc --version
cmake --version
