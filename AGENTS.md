# Working on this repo

Operational handoff notes. The public story is in `README.md`; this file is what
a new contributor (human or agent) needs beyond it.

## Commands

All from the repo root. The `tooling/*.sh` scripts re-enter the
`pico-screen-build` distrobox themselves, so run them from the host.

| Command | Purpose |
| --- | --- |
| `bash tooling/setup.sh` | create/refresh the distrobox and toolchain (uses host podman) |
| `bash tooling/build.sh [cmake args]` | build firmware, validate the UF2, print size and sha256 |
| `bash tooling/test.sh` | clang-format check (no auto-fix), pytest suite, ASan/UBSan fuzzing |
| `python3 tooling/flash.py` | flash to a mounted RPI-RP2 (BOOTSEL) drive; mounts it if needed |
| `bash tooling/probe-host.sh`, then `sudo build/probe-host` | device diagnostics |
| `sudo build/poll-host 6` | frame counter at high rate, for timing screen updates |

`tooling/test.sh` must pass before flashing; it enforces `clang-format --Werror`
on `firmware/` and the protocol test harness.

## Current device state

* The Pico W on this machine runs firmware built from this repo at the default
  settings: 62.5 MHz SPI, 64 KiB transfer buffers, rotation 0, XRGB8888.
* Panel: LCDWIKI MSP3222 (no touch), wired as in the README.
* Verified with niri on Fedora (kernel 6.19).
* Datasheets for the parts are filed under `~/Documents/Datasheets/`.

## Traps

* **Do not advertise RGB565.** GUD reports a plane framebuffer-format change as
  a mode change, and page flips carry no `ALLOW_MODESET`, so the compositor's
  flips fail with `EINVAL` and the panel stays black while USB looks healthy.
  Full explanation in the README design notes.
* **Reflashing while a compositor runs leaves a phantom output** for the old DRM
  card until the compositor restarts. Its log will mention `(deleted)` devices;
  that is expected and harmless.
* **MISO reads constant 0x00 on the tested module** (RDID4 and status registers),
  so `probe-host` always reports controller ID 0 and nothing may depend on panel
  readback.
* `16d0:10a9` is the upstream GUD USB ID that the kernel driver matches; do not
  repurpose it for other devices.
