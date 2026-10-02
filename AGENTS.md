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

## Known opportunity: halve USB traffic (RGB565 on the wire)

The device advertises XRGB8888 only, so Linux sends 4 bytes per pixel over Full
Speed USB. The kernel can convert XRGB8888 to RGB565 in software for the wire,
but only when the device does *not* advertise XRGB8888 — `gud_flush_damage()`:

```c
format = fb->format;
if (format->format == DRM_FORMAT_XRGB8888 && gdrm->xrgb8888_emulation_format)
        format = gdrm->xrgb8888_emulation_format;
```

So advertising RGB565 would roughly halve the pixel payload while compositors
keep rendering XRGB8888 — the kernel converts. Measured today: full-screen
changes send five 65 KiB chunks; that is the thing to shrink.

What blocks it: `gud_plane_atomic_check()` compares the plane's **raw**
framebuffer formats:

```c
if (old_fb && old_fb->format != format)
        crtc_state->mode_changed = true;
```

A page flip carries no `ALLOW_MODESET` (smithay uses `PAGE_FLIP_EVENT |
NONBLOCK`), and fbcon allocates an RGB565 framebuffer whenever RGB565 is
advertised (`preferred_depth = 16`). The compositor's XRGB8888 flips are then
rejected with `EINVAL`, permanently — that is the failure this repo hit and
worked around by advertising a single format.

Directions to investigate (untested):

1. Keep fbdev/fbcon from allocating a framebuffer on the GUD device, so the
   compositor's initial modeset installs its framebuffer onto an empty plane and
   later flips match. Look for a way to skip `drm_client_setup()` for `gud`.
2. Make the compositor scan out RGB565 too. Smithay/niri may not support a
   16-bpp scanout buffer today.
3. Kernel quirk: do not force a mode change when only the emulated format
   differs.

Verify with `/sys/kernel/debug/dri/<gud>/framebuffer` (each framebuffer's
`allocated by` and format), the `probe-host` counters, and the compositor log —
page-flip `EINVAL` is the failure signature.

## Investigated, no benefit: `gud.async_flush`

The kernel `gud` driver has a runtime-writable module parameter
(`/sys/module/gud/parameters/async_flush`) that merges damage into one bounding
box and flushes from a workqueue instead of synchronously in the commit.
Measured with two independent terminals updating on the panel (small scattered
rectangles, ~45 flushes/s): **44.9 flushes/s off, 47.5/s on, 43.3/s off** — no
meaningful difference. The compositor already coalesces damage to its frame
cadence and the flushes keep up, so the merge window never has anything to
merge. Don't spend time on it; bytes per pixel is the lever that matters.

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
* **GUI test apps must exist on niri's PATH.** `kitty` here is mise-managed and
  is not visible to processes niri spawns (`sh: kitty: command not found`), so
  a spawned test window silently never appears. Use a system-path binary such
  as `/usr/bin/alacritty` (or give the full path) for spawned test windows.
* Spawning a window to exercise the panel steals keyboard focus; restore it
  afterwards with `niri msg action focus-window --id <previously focused id>`
  (record it before spawning — `niri msg focused-window` may be empty).
