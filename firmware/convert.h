#ifndef SCREEN_CONVERT_H
#define SCREEN_CONVERT_H

#include <stddef.h>
#include <stdint.h>

/* Convert a Linux XRGB8888 buffer (little-endian bytes B,G,R,X) to the panel's
 * RGB565, in place. Iteration is forward, so the read position is always ahead
 * of the write position and no pixel is overwritten before it is read.
 */
void screen_xrgb8888_to_rgb565(uint8_t *buffer, size_t pixels);

#endif
