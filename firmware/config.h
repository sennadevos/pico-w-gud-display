#ifndef SCREEN_CONFIG_H
#define SCREEN_CONFIG_H

#ifndef SCREEN_ROTATION
#define SCREEN_ROTATION 0
#endif
#ifndef SCREEN_SPI_HZ
#define SCREEN_SPI_HZ 10000000
#endif

/* GPIO numbers, not physical header positions. */
#define SCREEN_PIN_MISO 16
#define SCREEN_PIN_CS 17
#define SCREEN_PIN_SCK 18
#define SCREEN_PIN_MOSI 19
#define SCREEN_PIN_DC 20
#define SCREEN_PIN_RESET 21
#define SCREEN_PIN_BACKLIGHT 22

#if SCREEN_ROTATION == 90 || SCREEN_ROTATION == 270
#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240
#define SCREEN_WIDTH_MM 65
#define SCREEN_HEIGHT_MM 49
#else
#define SCREEN_WIDTH 240
#define SCREEN_HEIGHT 320
#define SCREEN_WIDTH_MM 49
#define SCREEN_HEIGHT_MM 65
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
