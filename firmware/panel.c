#include "panel.h"

#include "config.h"
#include "hardware/gpio.h"
#include "hardware/pwm.h"
#include "hardware/spi.h"
#include "pico/stdlib.h"

#include <assert.h>

/* Blocking SPI throughout: a full 240x320 RGB565 frame takes ~140 ms at the
 * panel's 10 MHz limit, which is far inside every USB timeout and removes the
 * DMA/FIFO interaction from the bring-up path entirely.
 */
static bool display_enabled;
static unsigned int brightness = SCREEN_BACKLIGHT_DEFAULT;
static struct panel_probe probe;
static uint32_t actual_spi_hz;

/* LCDWIKI's exact MSP3222/MSP3223 IPS sequence, not a generic TN sequence.
 * https://www.lcdwiki.com/res/MSP3222_MSP3223/ILI9341V_Init.txt
 */
struct panel_init_command {
    uint8_t command;
    uint8_t length;
    uint8_t data[15];
};

static const struct panel_init_command init_commands[] = {
    {0xcf, 3, {0x00, 0xc1, 0x30}},
    {0xed, 4, {0x64, 0x03, 0x12, 0x81}},
    {0xe8, 3, {0x85, 0x00, 0x78}},
    {0xcb, 5, {0x39, 0x2c, 0x00, 0x34, 0x02}},
    {0xf7, 1, {0x20}},
    {0xea, 2, {0x00, 0x00}},
    {0xc0, 1, {0x13}},
    {0xc1, 1, {0x13}},
    {0xc5, 2, {0x1c, 0x35}},
    {0xc7, 1, {0xc8}},
    {0x21, 0, {0}}, /* IPS panel needs display inversion enabled. */
    {0xb6, 2, {0x0a, 0xa2}},
    {0x3a, 1, {0x55}}, /* RGB565 */
    {0xf6, 2, {0x01, 0x30}},
    {0xb1, 2, {0x00, 0x1b}},
    {0xf2, 1, {0x00}},
    {0x26, 1, {0x01}},
    {0xe0,
     15,
     {0x0f, 0x35, 0x31, 0x0b, 0x0e, 0x06, 0x49, 0xa7, 0x33, 0x07, 0x0f, 0x03, 0x0c, 0x0a, 0x00}},
    {0xe1,
     15,
     {0x00, 0x0a, 0x0f, 0x04, 0x11, 0x08, 0x36, 0x58, 0x4d, 0x07, 0x10, 0x0c, 0x32, 0x34, 0x0f}},
};

void panel_wait(void) {
    /* Nothing is left in flight after spi_write*_blocking returns. */
    while (spi_is_busy(spi0))
        tight_loop_contents();
}

static void command(uint8_t value, const uint8_t *data, size_t length) {
    panel_wait();
    spi_set_format(spi0, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    spi_set_baudrate(spi0, SCREEN_SPI_HZ);
    gpio_put(SCREEN_PIN_CS, 0);
    gpio_put(SCREEN_PIN_DC, 0);
    spi_write_blocking(spi0, &value, 1);
    if (length) {
        gpio_put(SCREEN_PIN_DC, 1);
        spi_write_blocking(spi0, data, length);
    }
    gpio_put(SCREEN_PIN_CS, 1);
}

static void read_registers(uint8_t command_code, uint8_t *out, size_t length) {
    panel_wait();
    spi_set_format(spi0, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    spi_set_baudrate(spi0, 1000000); /* Read timing limit is lower than write. */
    gpio_put(SCREEN_PIN_CS, 0);
    gpio_put(SCREEN_PIN_DC, 0);
    spi_write_blocking(spi0, &command_code, 1);
    gpio_put(SCREEN_PIN_DC, 1);
    /* The controller shifts out on the falling edge; MOSI is clocked with 0. */
    spi_read_blocking(spi0, 0, out, length);
    gpio_put(SCREEN_PIN_CS, 1);
}

static void probe_panel(void) {
    /* RDID4 returns a dummy byte, revision, then 0x93 and 0x41. */
    read_registers(0xd3, probe.rdid4, sizeof(probe.rdid4));
    read_registers(0xd3, probe.rdid4_repeat, sizeof(probe.rdid4_repeat));
    for (size_t i = 0; i < sizeof(probe.status); ++i) {
        uint8_t code = (uint8_t)(0x0a + i);
        read_registers(code, &probe.status[i], 1);
    }
    probe.controller_id =
        ((uint32_t)probe.rdid4[1] << 16) | ((uint32_t)probe.rdid4[2] << 8) | probe.rdid4[3];
}

static void update_backlight(void) {
    /* GUD brightness is perceptual. Keep 0% dim but on; DPMS turns it off. */
    unsigned int level = brightness ? brightness : 1;
    uint16_t duty = (uint16_t)((level * level * 65535u) / 10000u);
    pwm_set_gpio_level(SCREEN_PIN_BACKLIGHT, display_enabled ? duty : 0);
}

void panel_set_enabled(bool enabled) {
    display_enabled = enabled;
    update_backlight();
}

void panel_set_brightness(unsigned int percent) {
    assert(percent <= 100);
    brightness = percent;
    update_backlight();
}

void panel_init(void) {
    const unsigned int output_pins[] = {SCREEN_PIN_CS, SCREEN_PIN_DC, SCREEN_PIN_RESET,
                                        SCREEN_PIN_BACKLIGHT};
    for (size_t i = 0; i < sizeof(output_pins) / sizeof(output_pins[0]); ++i) {
        gpio_init(output_pins[i]);
        gpio_put(output_pins[i], output_pins[i] != SCREEN_PIN_BACKLIGHT);
        gpio_set_dir(output_pins[i], GPIO_OUT);
    }
    actual_spi_hz = spi_init(spi0, SCREEN_SPI_HZ);
    spi_set_format(spi0, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    gpio_set_function(SCREEN_PIN_SCK, GPIO_FUNC_SPI);
    gpio_set_function(SCREEN_PIN_MOSI, GPIO_FUNC_SPI);
    gpio_set_function(SCREEN_PIN_MISO, GPIO_FUNC_SPI);

    pwm_config pwm = pwm_get_default_config();
    pwm_config_set_wrap(&pwm, 65535);
    pwm_init(pwm_gpio_to_slice_num(SCREEN_PIN_BACKLIGHT), &pwm, true);
    pwm_set_gpio_level(SCREEN_PIN_BACKLIGHT, 0);
    gpio_set_function(SCREEN_PIN_BACKLIGHT, GPIO_FUNC_PWM);

    sleep_ms(50);
    gpio_put(SCREEN_PIN_RESET, 0);
    sleep_ms(100);
    gpio_put(SCREEN_PIN_RESET, 1);
    sleep_ms(50);

    probe.spi_hz = actual_spi_hz;
    probe_panel();

    /* An unavailable MISO read must not prevent write-only operation. */
    for (size_t i = 0; i < sizeof(init_commands) / sizeof(init_commands[0]); ++i)
        command(init_commands[i].command, init_commands[i].data, init_commands[i].length);

#if SCREEN_ROTATION == 0
    const uint8_t address_mode = 0x08;
#elif SCREEN_ROTATION == 90
    const uint8_t address_mode = 0x68;
#elif SCREEN_ROTATION == 180
    const uint8_t address_mode = 0xc8;
#else
    const uint8_t address_mode = 0xa8;
#endif
    command(0x36, &address_mode, 1);
    command(0x11, NULL, 0);
    sleep_ms(120);
    command(0x29, NULL, 0);
    sleep_ms(20);
}

void panel_write(uint16_t x, uint16_t y, uint16_t width, uint16_t height, const uint16_t *pixels) {
    assert(width && height && x + width <= SCREEN_WIDTH && y + height <= SCREEN_HEIGHT);
    panel_wait();
    uint16_t end_x = x + width - 1;
    uint16_t end_y = y + height - 1;
    uint8_t columns[] = {x >> 8, x & 0xff, end_x >> 8, end_x & 0xff};
    uint8_t pages[] = {y >> 8, y & 0xff, end_y >> 8, end_y & 0xff};
    command(0x2a, columns, sizeof(columns));
    command(0x2b, pages, sizeof(pages));

    gpio_put(SCREEN_PIN_CS, 0);
    gpio_put(SCREEN_PIN_DC, 0);
    const uint8_t write_memory = 0x2c;
    spi_write_blocking(spi0, &write_memory, 1);
    gpio_put(SCREEN_PIN_DC, 1);
    /* Native little-endian RGB565 words become MSB-first bytes on the wire. */
    spi_set_format(spi0, 16, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    actual_spi_hz = spi_set_baudrate(spi0, SCREEN_SPI_HZ);
    spi_write16_blocking(spi0, pixels, (size_t)width * height);
    gpio_put(SCREEN_PIN_CS, 1);
}

void panel_test_pattern(uint16_t *buffer, size_t buffer_size) {
    const uint16_t colors[] = {0xf800, 0x07e0, 0x001f, 0xffff, 0x0000, 0xffe0};
    const unsigned int rows = buffer_size / (SCREEN_WIDTH * sizeof(*buffer));
    assert(rows > 0);

    for (unsigned int y = 0; y < SCREEN_HEIGHT; y += rows) {
        unsigned int height = SCREEN_HEIGHT - y;
        if (height > rows)
            height = rows;
        for (unsigned int line = 0; line < height; ++line) {
            for (unsigned int x = 0; x < SCREEN_WIDTH; ++x) {
                unsigned int screen_y = y + line;
                uint16_t color = colors[(x * 6) / SCREEN_WIDTH];
                if (x < 2 || x >= SCREEN_WIDTH - 2 || screen_y < 2 || screen_y >= SCREEN_HEIGHT - 2)
                    color = 0xffff;
                if (screen_y > SCREEN_HEIGHT * 3 / 4 && ((x / 16 + screen_y / 16) % 2))
                    color = 0x8410;
                buffer[line * SCREEN_WIDTH + x] = color;
            }
        }
        panel_write(0, y, SCREEN_WIDTH, height, buffer);
    }
    panel_wait();
}

const struct panel_probe *panel_probe_result(void) { return &probe; }
uint32_t panel_spi_hz(void) { return actual_spi_hz; }
