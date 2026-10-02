#ifndef SCREEN_PANEL_H
#define SCREEN_PANEL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Raw SPI readback, so a dead line is distinguishable from a floating one. */
struct panel_probe {
    uint32_t controller_id;
    uint32_t spi_hz;
    uint8_t rdid4[4];
    uint8_t rdid4_repeat[4];
    uint8_t status[6]; /* registers 0x0a..0x0f */
};

void panel_init(void);
void panel_wait(void);
void panel_write(uint16_t x, uint16_t y, uint16_t width, uint16_t height, const uint16_t *pixels);
void panel_test_pattern(uint16_t *buffer, size_t buffer_size);
void panel_set_enabled(bool enabled);
void panel_set_brightness(unsigned int percent);
const struct panel_probe *panel_probe_result(void);
uint32_t panel_spi_hz(void);

#endif
