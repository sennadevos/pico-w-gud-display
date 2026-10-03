#ifndef SCREEN_CONFIG_H
#define SCREEN_CONFIG_H

#include "board.h"

#ifndef SCREEN_ROTATION
#define SCREEN_ROTATION 0
#endif
#ifndef SCREEN_SPI_HZ
#define SCREEN_SPI_HZ 10000000
#endif

/* The board defines its own SPI instance (the pins dictate which one). */
#if SCREEN_SPI_INDEX == 1
#define SCREEN_SPI spi1
#else
#define SCREEN_SPI spi0
#endif

/* Rotation swaps the panel's base geometry; see board.h for the base. */
#if SCREEN_ROTATION == 90 || SCREEN_ROTATION == 270
#define SCREEN_WIDTH SCREEN_BASE_HEIGHT
#define SCREEN_HEIGHT SCREEN_BASE_WIDTH
#define SCREEN_WIDTH_MM SCREEN_BASE_HEIGHT_MM
#define SCREEN_HEIGHT_MM SCREEN_BASE_WIDTH_MM
#else
#define SCREEN_WIDTH SCREEN_BASE_WIDTH
#define SCREEN_HEIGHT SCREEN_BASE_HEIGHT
#define SCREEN_WIDTH_MM SCREEN_BASE_WIDTH_MM
#define SCREEN_HEIGHT_MM SCREEN_BASE_HEIGHT_MM
#endif

/* Two bounded transfer buffers instead of two full framebuffers (307200 B).
 * 64 KiB covers 68 full-width rows of XRGB8888 (65280 B), so the kernel splits
 * a full-screen change into five chunks instead of ten.
 */
#define SCREEN_BUFFER_SIZE 65536
#define SCREEN_BACKLIGHT_DEFAULT 100

#define SCREEN_USB_VID 0x16d0
#define SCREEN_USB_PID 0x10a9
#define SCREEN_USB_INTERFACE 0
#define SCREEN_USB_BULK_OUT 0x03

/* Read-only bring-up diagnostics; outside the standard GUD request range. */
#define SCREEN_REQ_GET_DIAGNOSTICS 0x70

#endif
