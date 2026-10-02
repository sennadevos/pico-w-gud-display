#include "convert.h"

void screen_xrgb8888_to_rgb565(uint8_t *buffer, size_t pixels) {
    for (size_t i = 0; i < pixels; ++i) {
        uint8_t blue = buffer[i * 4 + 0];
        uint8_t green = buffer[i * 4 + 1];
        uint8_t red = buffer[i * 4 + 2];
        uint16_t rgb565 =
            (uint16_t)(((uint16_t)(red >> 3) << 11) | ((uint16_t)(green >> 2) << 5) | (blue >> 3));
        buffer[i * 2 + 0] = (uint8_t)rgb565;        /* low byte first in memory */
        buffer[i * 2 + 1] = (uint8_t)(rgb565 >> 8); /* sent MSB-first on the wire */
    }
}
