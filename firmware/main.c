#include "config.h"
#include "convert.h"
#include "gud_usb.h"
#include "panel.h"
#include "pico/binary_info.h"
#include "pico/stdlib.h"
#include "pico/unique_id.h"
#include "tusb.h"

#include <string.h>

static uint16_t pixels[SCREEN_BUFFER_SIZE / sizeof(uint16_t)] __attribute__((aligned(4)));
static uint8_t compressed[SCREEN_BUFFER_SIZE] __attribute__((aligned(4)));

static const uint8_t pixel_formats[] = {GUD_PIXEL_FORMAT_XRGB8888};
static const struct gud_property_req connector_properties[] = {
    {.prop = GUD_PROPERTY_BACKLIGHT_BRIGHTNESS, .val = SCREEN_BACKLIGHT_DEFAULT},
};

static uint32_t serial_number(void) {
    pico_unique_board_id_t id;
    uint32_t serial;
    pico_get_unique_board_id(&id);
    memcpy(&serial, id.id, sizeof(serial));
    return serial;
}

/* These describe a virtual USB mode, not SPI/frame-update throughput. */
static struct gud_display_timings timings = {
    .hfront = 8,
    .hsync = 8,
    .hback = 16,
    .vfront = 4,
    .vsync = 4,
    .vback = 8,
    .framerate = 30,
};

static const struct gud_display_edid edid = {
    .name = "PicoW-TFT",
    .pnp = "GUD",
    .product_code = 0x3222,
    .year = 2026,
    .width_mm = SCREEN_WIDTH_MM,
    .height_mm = SCREEN_HEIGHT_MM,
    .gamma = 220,
    .timings = &timings,
    .get_serial_number = serial_number,
};

static int controller_enable(const struct gud_display *display, uint8_t enabled) {
    (void)display;
    if (!enabled)
        panel_wait();
    return 0;
}

static int display_enable(const struct gud_display *display, uint8_t enabled) {
    (void)display;
    panel_set_enabled(enabled != 0);
    return 0;
}

static int state_commit(const struct gud_display *display, const struct gud_state_req *state,
                        uint8_t property_count) {
    (void)display;
    for (uint8_t i = 0; i < property_count; ++i) {
        const struct gud_property_req *property = &state->properties[i];
        if (property->prop == GUD_PROPERTY_BACKLIGHT_BRIGHTNESS)
            panel_set_brightness((unsigned int)property->val);
    }
    return 0;
}

static int set_buffer(const struct gud_display *display, const struct gud_set_buffer_req *buffer) {
    (void)display;
    (void)buffer;
    panel_wait(); /* USB must not overwrite pixels still being read by DMA. */
    return 0;
}

static void write_buffer(const struct gud_display *display, const struct gud_set_buffer_req *buffer,
                         uint8_t format, void *data) {
    (void)display;
    if (format == GUD_PIXEL_FORMAT_XRGB8888)
        screen_xrgb8888_to_rgb565(data, (size_t)buffer->width * buffer->height);
    panel_write(buffer->x, buffer->y, buffer->width, buffer->height, data);
}

static const struct gud_display display = {
    .width = SCREEN_WIDTH,
    .height = SCREEN_HEIGHT,
    .flags = GUD_DISPLAY_FLAG_STATUS_ON_SET,
    .compression = GUD_COMPRESSION_LZ4,
    .max_buffer_size = SCREEN_BUFFER_SIZE,
    .formats = pixel_formats,
    .num_formats = 1,
    .connector_properties = connector_properties,
    .num_connector_properties = 1,
    .edid = &edid,
    .controller_enable = controller_enable,
    .display_enable = display_enable,
    .state_commit = state_commit,
    .set_buffer = set_buffer,
    .write_buffer = write_buffer,
};

void tud_mount_cb(void) {
    /* Do not power the backlight until USB grants the requested 250 mA. */
    panel_set_enabled(true);
}

void tud_umount_cb(void) {
    panel_wait();
    panel_set_enabled(false);
}

void tud_suspend_cb(bool remote_wakeup_enabled) {
    (void)remote_wakeup_enabled;
    panel_wait();
    panel_set_enabled(false);
}

void tud_resume_cb(void) { panel_set_enabled(true); }

int main(void) {
    bi_decl(bi_program_description("RP2040 Pico W, LCDWIKI MSP3222/MSP3223, native Linux GUD"));
    bi_decl(bi_1pin_with_name(SCREEN_PIN_MISO, "TFT MISO"));
    bi_decl(bi_1pin_with_name(SCREEN_PIN_CS, "TFT CS"));
    bi_decl(bi_1pin_with_name(SCREEN_PIN_SCK, "TFT SCK"));
    bi_decl(bi_1pin_with_name(SCREEN_PIN_MOSI, "TFT MOSI"));
    bi_decl(bi_1pin_with_name(SCREEN_PIN_DC, "TFT RS/DC"));
    bi_decl(bi_1pin_with_name(SCREEN_PIN_RESET, "TFT RST"));
    bi_decl(bi_1pin_with_name(SCREEN_PIN_BACKLIGHT, "TFT LED control"));

    panel_init();
    panel_test_pattern(pixels, sizeof(pixels));
    /* Light the bring-up image immediately: a dark panel during the short
     * pre-enumeration window is indistinguishable from a wiring fault.
     */
    panel_set_enabled(true);
    gud_usb_setup(&display, pixels, compressed);
    tusb_init();
    while (true)
        tud_task();
}
