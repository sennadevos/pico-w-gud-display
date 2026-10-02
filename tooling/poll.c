/* High-rate frame-counter monitor for the Pico W GUD display.
 *
 * probe-host spawns a process per sample (~200 ms); this keeps one libusb
 * session open and samples the firmware's diagnostics counters every few
 * milliseconds, so screen-update bursts can be timed precisely.
 *
 *   sudo build/poll-host [seconds]
 */
#include <libusb-1.0/libusb.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define SCREEN_REQ_GET_DIAGNOSTICS 0x70

struct screen_diagnostics {
    uint32_t version, controller_id, spi_hz, completed, failed, width, height, reserved;
    uint8_t rdid4[4], rdid4_repeat[4], status[6], padding[2];
} __attribute__((packed));

int main(int argc, char **argv) {
    double seconds = argc > 1 ? atof(argv[1]) : 5.0;
    libusb_context *ctx = NULL;
    libusb_device_handle *dev;
    struct screen_diagnostics d;
    struct timespec t0, now;
    uint32_t last = 0xFFFFFFFF;
    unsigned long samples = 0;

    if (libusb_init(&ctx) != 0)
        return 1;
    dev = libusb_open_device_with_vid_pid(ctx, 0x16d0, 0x10a9);
    if (!dev) {
        fprintf(stderr, "cannot open 16d0:10a9 (need root)\n");
        libusb_exit(ctx);
        return 1;
    }

    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (;;) {
        clock_gettime(CLOCK_MONOTONIC, &now);
        double elapsed = (now.tv_sec - t0.tv_sec) + (now.tv_nsec - t0.tv_nsec) / 1e9;
        if (elapsed > seconds)
            break;
        if (libusb_control_transfer(dev, 0xC1, SCREEN_REQ_GET_DIAGNOSTICS, 0, 0,
                                    (uint8_t *)&d, sizeof(d), 500) == (int)sizeof(d)) {
            if (d.completed != last) {
                printf("%9.3f %u%s\n", elapsed, d.completed,
                       last == 0xFFFFFFFF ? " (start)" : "");
                fflush(stdout);
                last = d.completed;
            }
            ++samples;
        }
    }
    fprintf(stderr, "%lu samples, final %u frames, %u failed\n", samples, d.completed, d.failed);
    libusb_close(dev);
    libusb_exit(ctx);
    return 0;
}
