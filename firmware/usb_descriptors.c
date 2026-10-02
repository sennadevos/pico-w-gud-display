#include "config.h"
#include "pico/unique_id.h"
#include "tusb.h"

#include <string.h>

static const tusb_desc_device_t device_descriptor = {
    .bLength = sizeof(tusb_desc_device_t),
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = 0x0200,
    .bDeviceClass = 0,
    .bDeviceSubClass = 0,
    .bDeviceProtocol = 0,
    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor = SCREEN_USB_VID,
    .idProduct = SCREEN_USB_PID,
    .bcdDevice = 0x0100,
    .iManufacturer = 1,
    .iProduct = 2,
    .iSerialNumber = 3,
    .bNumConfigurations = 1,
};

/* USB-powered: reserve 250 mA for Pico W and the approximately 95 mA backlight.
 * The Wi-Fi radio is not initialized or used by this firmware.
 */
static const uint8_t configuration_descriptor[] = {
    // clang-format off
    TUD_CONFIG_DESCRIPTOR(1, 1, 0, TUD_CONFIG_DESC_LEN + 9 + 7, 0, 250),
    9, TUSB_DESC_INTERFACE, SCREEN_USB_INTERFACE, 0, 1,
    TUSB_CLASS_VENDOR_SPECIFIC, 0, 0, 4,
    7, TUSB_DESC_ENDPOINT, SCREEN_USB_BULK_OUT, TUSB_XFER_BULK, 64, 0, 0,
    // clang-format on
};

uint8_t const *tud_descriptor_device_cb(void) { return (const uint8_t *)&device_descriptor; }

uint8_t const *tud_descriptor_configuration_cb(uint8_t index) {
    return index == 0 ? configuration_descriptor : NULL;
}

uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
    (void)langid;
    static uint16_t descriptor[32];
    char serial[PICO_UNIQUE_BOARD_ID_SIZE_BYTES * 2 + 1];
    const char *strings[] = {NULL, "Pico W project", "Pico W LCDWIKI Display", serial,
                             "GUD Display"};
    size_t length;

    if (index == 0) {
        descriptor[1] = 0x0409;
        length = 1;
    } else {
        if (index >= sizeof(strings) / sizeof(strings[0]))
            return NULL;
        if (index == 3)
            pico_get_unique_board_id_string(serial, sizeof(serial));
        length = strlen(strings[index]);
        if (length > 31)
            length = 31;
        for (size_t i = 0; i < length; ++i)
            descriptor[1 + i] = (uint8_t)strings[index][i];
    }
    descriptor[0] = (TUSB_DESC_STRING << 8) | (2 * length + 2);
    return descriptor;
}
