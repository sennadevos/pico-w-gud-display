/* GUD transport derived from notro/gud-pico's MIT-licensed TinyUSB driver.
 * Copyright (c) 2021-2024 Noralf Trønnes
 * Uses bounded, single-rectangle transfers and TinyUSB's current callbacks.
 */
#include "gud_usb.h"

#include "config.h"
#include "device/usbd_pvt.h"
#include "lz4.h"
#include "panel.h"
#include "pico/stdlib.h"
#include "tusb.h"

#include <string.h>

static const struct gud_display *display;
static uint8_t *pixel_buffer;
static uint8_t *compressed_buffer;
static uint8_t endpoint;
static uint8_t last_status;
static uint32_t expected_transfer;
static struct gud_set_buffer_req pending_buffer;
static bool transfer_pending;
static uint32_t completed_updates;
static uint32_t failed_updates;

CFG_TUSB_MEM_ALIGN static uint8_t control_buffer[128];

static void driver_init(void) {
    endpoint = 0;
    last_status = GUD_STATUS_OK;
    transfer_pending = false;
}

static bool driver_deinit(void) {
    panel_wait();
    transfer_pending = false;
    return true;
}

static void driver_reset(uint8_t rhport) {
    (void)rhport;
    panel_wait();
    endpoint = 0;
    transfer_pending = false;
    last_status = GUD_STATUS_OK;
    gud_reset_state();
}

static uint16_t driver_open(uint8_t rhport, const tusb_desc_interface_t *descriptor,
                            uint16_t max_length) {
    const uint16_t length = sizeof(*descriptor) + sizeof(tusb_desc_endpoint_t);
    if (descriptor->bInterfaceClass != TUSB_CLASS_VENDOR_SPECIFIC ||
        descriptor->bInterfaceNumber != SCREEN_USB_INTERFACE || descriptor->bNumEndpoints != 1 ||
        max_length < length)
        return 0;

    const tusb_desc_endpoint_t *bulk = (const tusb_desc_endpoint_t *)tu_desc_next(descriptor);
    if (bulk->bDescriptorType != TUSB_DESC_ENDPOINT ||
        bulk->bEndpointAddress != SCREEN_USB_BULK_OUT ||
        bulk->bmAttributes.xfer != TUSB_XFER_BULK || !usbd_edpt_open(rhport, bulk))
        return 0;
    endpoint = bulk->bEndpointAddress;
    return length;
}

static bool set_request(uint8_t rhport, const tusb_control_request_t *request) {
    if (request->bRequest == GUD_REQ_SET_BUFFER && !endpoint) {
        last_status = GUD_STATUS_ERROR;
        return false;
    }
    if (request->bRequest == GUD_REQ_SET_BUFFER &&
        (transfer_pending || usbd_edpt_busy(rhport, endpoint))) {
        last_status = GUD_STATUS_BUSY;
        return false;
    }
    int result =
        gud_req_set(display, request->bRequest, request->wValue, control_buffer, request->wLength);
    if (result < 0) {
        last_status = (uint8_t)-result;
        return false;
    }

    if (request->bRequest != GUD_REQ_SET_BUFFER)
        return true;

    memcpy(&pending_buffer, control_buffer, sizeof(pending_buffer));
    expected_transfer =
        pending_buffer.compression ? pending_buffer.compressed_length : pending_buffer.length;
    /* gud_req_set() validated lengths and waited for the previous SPI DMA. */
    uint8_t *buffer = pending_buffer.compression ? compressed_buffer : pixel_buffer;
    transfer_pending = true;
    if (!usbd_edpt_xfer(rhport, endpoint, buffer, (uint16_t)expected_transfer)) {
        transfer_pending = false;
        last_status = GUD_STATUS_ERROR;
        ++failed_updates;
        return false;
    }
    return true;
}

static size_t diagnostics(void) {
    /* Packed little-endian layout; see struct screen_diagnostics in probe.c. */
    const struct panel_probe *p = panel_probe_result();
    struct __attribute__((packed)) {
        uint32_t version, controller_id, spi_hz, completed, failed, width, height, reserved;
        uint8_t rdid4[4], rdid4_repeat[4], status[6], padding[2];
    } out = {
        .version = 2,
        .controller_id = p->controller_id,
        .spi_hz = p->spi_hz,
        .completed = completed_updates,
        .failed = failed_updates,
        .width = SCREEN_WIDTH,
        .height = SCREEN_HEIGHT,
    };
    memcpy(out.rdid4, p->rdid4, sizeof(out.rdid4));
    memcpy(out.rdid4_repeat, p->rdid4_repeat, sizeof(out.rdid4_repeat));
    memcpy(out.status, p->status, sizeof(out.status));
    memcpy(control_buffer, &out, sizeof(out));
    return sizeof(out);
}

static bool driver_control(uint8_t rhport, uint8_t stage, const tusb_control_request_t *request) {
    if (request->bmRequestType_bit.type != TUSB_REQ_TYPE_VENDOR ||
        request->bmRequestType_bit.recipient != TUSB_REQ_RCPT_INTERFACE ||
        request->wIndex != SCREEN_USB_INTERFACE)
        return false;

    if (stage == CONTROL_STAGE_SETUP) {
        if (request->bmRequestType_bit.direction == TUSB_DIR_IN) {
            if (request->bRequest == GUD_REQ_GET_STATUS)
                return request->wValue == 0 && request->wLength == 1 &&
                       tud_control_xfer(rhport, request, &last_status, 1);

            last_status = GUD_STATUS_OK;
            int length;
            if (request->bRequest == SCREEN_REQ_GET_DIAGNOSTICS) {
                if (request->wValue || !request->wLength)
                    length = -GUD_STATUS_PROTOCOL_ERROR;
                else
                    length = (int)diagnostics();
            } else {
                size_t capacity = request->wLength;
                if (capacity > sizeof(control_buffer))
                    capacity = sizeof(control_buffer);
                length = gud_req_get(display, request->bRequest, request->wValue, control_buffer,
                                     capacity);
            }
            if (length < 0) {
                last_status = (uint8_t)-length;
                return false;
            }
            return tud_control_xfer(rhport, request, control_buffer, (uint16_t)length);
        }

        last_status = GUD_STATUS_OK;
        if (request->wLength > sizeof(control_buffer)) {
            last_status = GUD_STATUS_PROTOCOL_ERROR;
            return false;
        }
        if (!request->wLength) {
            if (!set_request(rhport, request))
                return false;
            return tud_control_status(rhport, request);
        }
        return tud_control_xfer(rhport, request, control_buffer, request->wLength);
    }

    if (stage == CONTROL_STAGE_DATA && request->bmRequestType_bit.direction == TUSB_DIR_OUT)
        return set_request(rhport, request);

    return true;
}

/* TinyUSB routes all vendor requests here, including interface recipients. */
bool tud_vendor_control_xfer_cb(uint8_t rhport, uint8_t stage,
                                const tusb_control_request_t *request) {
    return driver_control(rhport, stage, request);
}

static bool driver_transfer(uint8_t rhport, uint8_t ep_addr, xfer_result_t result,
                            uint32_t transferred) {
    (void)rhport;
    if (ep_addr != endpoint || !transfer_pending)
        return false;
    transfer_pending = false;
    if (result != XFER_RESULT_SUCCESS || transferred != expected_transfer) {
        last_status = GUD_STATUS_ERROR;
        ++failed_updates;
        return false;
    }
    if (pending_buffer.compression) {
        int decoded = LZ4_decompress_safe((const char *)compressed_buffer, (char *)pixel_buffer,
                                          (int)transferred, (int)pending_buffer.length);
        if (decoded != (int)pending_buffer.length) {
            last_status = GUD_STATUS_ERROR;
            ++failed_updates;
            return false;
        }
    }
    gud_write_buffer(display, pixel_buffer);
    ++completed_updates;
    return true;
}

static const usbd_class_driver_t class_driver = {
    .name = "GUD",
    .init = driver_init,
    .deinit = driver_deinit,
    .reset = driver_reset,
    .open = driver_open,
    .control_xfer_cb = driver_control,
    .xfer_cb = driver_transfer,
    .sof = NULL,
};

const usbd_class_driver_t *usbd_app_driver_get_cb(uint8_t *count) {
    *count = 1;
    return &class_driver;
}

void gud_usb_setup(const struct gud_display *configuration, void *pixels, void *compressed) {
    display = configuration;
    pixel_buffer = pixels;
    compressed_buffer = compressed;
    gud_reset_state();
}
