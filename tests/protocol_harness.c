#include "gud.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static const uint8_t formats[] = {GUD_PIXEL_FORMAT_XRGB8888};
static const struct gud_property_req properties[] = {
    {.prop = GUD_PROPERTY_BACKLIGHT_BRIGHTNESS, .val = 100},
};
static const struct gud_display_edid edid = {
    .name = "PicoW-TFT",
    .pnp = "GUD",
    .product_code = 0x3222,
    .year = 2026,
    .width_mm = 49,
    .height_mm = 65,
};
static const struct gud_display display = {
    .width = 240,
    .height = 320,
    .flags = GUD_DISPLAY_FLAG_STATUS_ON_SET,
    .compression = GUD_COMPRESSION_LZ4,
    .max_buffer_size = 65536,
    .formats = formats,
    .num_formats = 1,
    .connector_properties = properties,
    .num_connector_properties = 1,
    .edid = &edid,
};

int protocol_get(uint8_t request, uint16_t index, void *data, size_t size) {
    return gud_req_get(&display, request, index, data, size);
}

int protocol_set(uint8_t request, uint16_t index, const void *data, size_t size) {
    return gud_req_set(&display, request, index, data, size);
}

void protocol_reset(void) { gud_reset_state(); }

/* Executed under ASan/UBSan as a separate test binary. */
int main(void) {
    uint8_t output[128] = {0};
    uint8_t request[128] = {0};
    protocol_reset();
    for (unsigned int code = 0; code < 256; ++code) {
        for (size_t length = 0; length <= sizeof(request); ++length) {
            /* Exact allocations detect reads beyond a malformed payload. */
            uint8_t *payload = malloc(length ? length : 1);
            memcpy(payload, request, length);
            protocol_set(code, 0, payload, length);
            free(payload);
            if (length)
                protocol_get(code, 0, output, length);
        }
    }
    return 0;
}
