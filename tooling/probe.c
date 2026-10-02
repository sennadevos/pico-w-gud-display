/* Static libusb probe for the Pico W GUD display.
 *
 * The distrobox cannot open the USB device node: rootless podman maps container
 * root to the unprivileged user. Build this static and run it with host sudo.
 *
 *   tooling/probe-host.sh
 */
#include <libusb-1.0/libusb.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define VID 0x16d0
#define PID 0x10a9
#define GUD_REQ_GET_DESCRIPTOR 0x01
#define SCREEN_REQ_GET_DIAGNOSTICS 0x70

struct screen_diagnostics {
    uint32_t version, controller_id, spi_hz, completed, failed, width, height, reserved;
    uint8_t rdid4[4], rdid4_repeat[4], status[6], padding[2];
} __attribute__((packed));

static int get(libusb_device_handle *dev, uint8_t request, uint16_t length, void *data) {
    int r = libusb_control_transfer(dev, 0xC1, request, 0, 0, data, length, 3000);
    return r < 0 ? r : (r == length ? 0 : -100);
}

static void hex(const char *label, const uint8_t *p, size_t n) {
    printf("%-16s:", label);
    for (size_t i = 0; i < n; ++i)
        printf(" %02x", p[i]);
    printf("\n");
}

int main(void) {
    libusb_context *ctx = NULL;
    libusb_device_handle *dev = NULL;
    uint8_t desc[30] = {0};
    struct screen_diagnostics d;
    int r;

    memset(&d, 0, sizeof(d));
    if (libusb_init(&ctx) != 0) {
        fprintf(stderr, "libusb_init failed\n");
        return 1;
    }
    dev = libusb_open_device_with_vid_pid(ctx, VID, PID);
    if (!dev) {
        fprintf(stderr, "Cannot open %04x:%04x (run as root)\n", VID, PID);
        libusb_exit(ctx);
        return 1;
    }

    r = get(dev, GUD_REQ_GET_DESCRIPTOR, sizeof(desc), desc);
    if (r != 0) {
        fprintf(stderr, "GET_DESCRIPTOR failed: %s\n", libusb_error_name(r));
        goto out;
    }
    uint32_t magic = desc[0] | (desc[1] << 8) | (desc[2] << 16) | (desc[3] << 24);
    printf("GUD magic       : %08x %s\n", magic, magic == 0x1d50614d ? "ok" : "WRONG");

    r = get(dev, SCREEN_REQ_GET_DIAGNOSTICS, sizeof(d), &d);
    if (r != 0) {
        fprintf(stderr, "GET_DIAGNOSTICS failed: %s\n", libusb_error_name(r));
        goto out;
    }
    printf("Controller ID   : 0x%06x %s\n", d.controller_id,
           (d.controller_id & 0xffff) == 0x9341 ? "(ILI9341V ok)" : "(UNEXPECTED)");
    printf("SPI clock       : %u Hz\n", d.spi_hz);
    printf("Frames written  : %u (failed %u)\n", d.completed, d.failed);
    printf("Panel size      : %ux%u\n", d.width, d.height);
    printf("\nRaw SPI readback:\n");
    hex("RDID4 1st", d.rdid4, 4);
    hex("RDID4 2nd", d.rdid4_repeat, 4);
    hex("regs 0x0a-0x0f", d.status, 6);

    printf("\nInterpretation: \n");
    int all_zero = 1, all_ff = 1;
    const uint8_t *all[] = {d.rdid4, d.rdid4_repeat, d.status};
    size_t sizes[] = {4, 4, 6};
    for (int i = 0; i < 3; ++i)
        for (size_t j = 0; j < sizes[i]; ++j) {
            all_zero &= all[i][j] == 0x00;
            all_ff &= all[i][j] == 0xff;
        }
    if ((d.controller_id & 0xffff) == 0x9341)
        printf("  Panel answered on MISO. SPI and the panel are working.\n");
    else if (all_zero)
        printf("  MISO reads constant 0x00: MISO disconnected, or the panel is held\n"
               "  in reset / never selected. Check MISO(16), CS(17), RST(21).\n");
    else if (all_ff)
        printf("  MISO reads constant 0xff: line floating high. Check MISO(16).\n");
    else
        printf("  Unstable values: MISO is floating or the SPI clock is marginal.\n");

out:
    libusb_close(dev);
    libusb_exit(ctx);
    return r == 0 ? 0 : 1;
}
