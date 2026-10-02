#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
source "$root/tooling/versions.env"

packages="build-essential ca-certificates clang-format cmake curl gcc-arm-none-eabi git libdrm-dev libdrm-tests liblz4-dev libnewlib-arm-none-eabi libstdc++-arm-none-eabi-newlib libusb-1.0-0-dev ninja-build pkg-config python3 python3-lz4 python3-pytest python3-usb"

if ! podman container exists "$DISTROBOX_NAME"; then
    distrobox create --yes --no-entry \
        --name "$DISTROBOX_NAME" \
        --image "$DISTROBOX_IMAGE" \
        --home "$root/.distrobox-home" \
        --additional-packages "$packages"
fi

distrobox enter --no-tty --clean-path --name "$DISTROBOX_NAME" -- \
    bash "$root/tooling/bootstrap.sh"
