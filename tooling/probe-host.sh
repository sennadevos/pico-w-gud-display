#!/usr/bin/env bash
# Build the static USB probe in the distrobox, then run it with host privileges.
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
source "$root/tooling/versions.env"

if [[ ! -f /run/.containerenv ]]; then
    exec distrobox enter --no-tty --clean-path --name "$DISTROBOX_NAME" -- \
        bash "$root/tooling/probe-host.sh"
fi

# libusb is linked statically; libudev dynamically, since Debian ships no .a.
cc -O2 -o "$root/build/probe-host" "$root/tooling/probe.c" \
    /usr/lib/x86_64-linux-gnu/libusb-1.0.a /usr/lib/x86_64-linux-gnu/libudev.so.1 -lpthread
cc -O2 -o "$root/build/poll-host" "$root/tooling/poll.c" \
    /usr/lib/x86_64-linux-gnu/libusb-1.0.a /usr/lib/x86_64-linux-gnu/libudev.so.1 -lpthread

echo "Built $root/build/probe-host and $root/build/poll-host"
echo "Run them with host privileges:"
echo "  sudo -n $root/build/probe-host          # full diagnostics"
echo "  sudo -n $root/build/poll-host 6         # frame counter, high rate"
