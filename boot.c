// SPDX-License-Identifier: MIT
/*
 * After the exploit: boot the phone from the images in our flash, the way
 * badapple's tools/iboot-boot.sh does from a computer.
 *
 * The pwned bootrom takes the patched iBSS over DFU requests and jumps to it.
 * iBSS comes back in recovery mode, where each image goes over bulk endpoint
 * 0x04 and a command string over EP0 tells iBoot what it was. The last image
 * is m1n1, which loads the rest from the phone's own NVMe.
 */
#include "boot.h"

#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"

#include "pio_usb.h"
#include "pio_usb_ll.h"
#include "usb_definitions.h"

#include "bus.h"
#include "led.h"
#include "log.h"
#include "usb.h"

#define IMAGE(name)                                                          \
    extern const uint8_t name##_start[], name##_end[];
IMAGE(ibss)
IMAGE(devicetree)
IMAGE(trustcache)
IMAGE(gfx)
IMAGE(kernelcache)

static const struct {
    const char *name;
    const uint8_t *start, *end;
    const char *cmd;
} images[] = {
    {"devicetree", devicetree_start, devicetree_end, "devicetree"},
    {"trustcache", trustcache_start, trustcache_end, "firmware"},
    // GPU firmware, empty when it was not built
    {"gfx", gfx_start, gfx_end, "firmware"},
    {"kernelcache", kernelcache_start, kernelcache_end, "bootx"},
};

// The exploit's DFU handler: download in chunks, then request 8 jumps. Its
// descriptor says 2048 bytes, but at full speed a download of more than three
// packets never gets its status stage: 192 works, 256 does not.
#define DFU_CHUNK   192
#define REQ_DNLOAD  1
#define REQ_CLRSTAT 4
#define REQ_ABORT   6
#define REQ_BOOT    8

#define REC_ADDR    1
#define REC_BULK_EP 0x04
#define BULK_CHUNK  0x4000

static int ctrl(bus_t *b, uint8_t type, uint8_t req, uint16_t value, uint16_t index,
                const void *data, uint16_t len, uint32_t timeout_ms) {
    uint8_t setup[8] = {type, req, value, value >> 8, index, index >> 8, len, len >> 8};
    return bus_control_xfer(b, setup, (uint8_t *)data, len, type & 0x80, timeout_ms);
}

// Address and configure the device the way a computer's USB stack would
static int usb_configure(bus_t *b) {
    if (ctrl(b, 0x00, 5, REC_ADDR, 0, NULL, 0, 500))
        return -1;
    sleep_ms(2);
    pio_usb_host_endpoint_close(0, 0, 0x00);
    b->dev->address = REC_ADDR;
    if (!bus_open_ep0(b, 64))
        return -1;
    return ctrl(b, 0x00, 9, 1, 0, NULL, 0, 500);
}

static int dfu_boot_ibss(bus_t *b, __unused void *ctx) {
    size_t len = ibss_end - ibss_start;

    // PIO USB encodes packets in its interrupt, so keep the data in RAM
    static uint8_t chunk[DFU_CHUNK];

    if (usb_configure(b)) {
        INFO("cannot configure the DFU device");
        return -1;
    }

    INFO("sending iBSS, %u bytes", len);
    for (size_t off = 0; off < len; off += DFU_CHUNK) {
        size_t n = len - off < DFU_CHUNK ? len - off : DFU_CHUNK;
        memcpy(chunk, ibss_start + off, n);
        int rc = ctrl(b, 0x21, REQ_DNLOAD, 0, 0, chunk, n, 5000);
        if (rc) {
            INFO("DFU download failed at 0x%x: %d, control stage %d", off, rc,
                 b->dev->control_pipe.stage);

            return -1;
        }
        if (off % 0x40080 == 0)
            INFO("  0x%x", off);
    }
    ctrl(b, 0x21, REQ_DNLOAD, 0, 0, NULL, 0, 1000);

    // Request 8 points DFU's return at the image, and leaving DFU is
    // usbliter8ctl's request 4. iBSS takes over before these finish, so
    // errors here are expected.
    ctrl(b, 0x21, REQ_BOOT, 0, 0, NULL, 0, 1000);
    ctrl(b, 0x21, REQ_CLRSTAT, 0, 0, NULL, 0, 1000);
    if (b->root->connected)
        ctrl(b, 0x21, REQ_ABORT, 0, 0, NULL, 0, 1000);
    return 0;
}

static endpoint_t *find_ep(uint8_t addr, uint8_t ep_num) {
    for (int i = 0; i < PIO_USB_EP_POOL_CNT; i++) {
        endpoint_t *ep = PIO_USB_ENDPOINT(i);
        if (ep->size && ep->dev_addr == addr && ep->ep_num == ep_num)
            return ep;
    }
    return NULL;
}

// The host IRQ clears the completion bits as it handles them, so watch the
// endpoint itself: done when it has no transfer, good when all of it went.
static int bulk_out(bus_t *b, const uint8_t *data, uint16_t len) {
    endpoint_t *ep = find_ep(REC_ADDR, REC_BULK_EP);
    if (!ep)
        return -1;

    if (!pio_usb_host_endpoint_transfer(0, REC_ADDR, REC_BULK_EP, (uint8_t *)data, len))
        return -2;

    absolute_time_t deadline = make_timeout_time_ms(5000);
    while (ep->has_transfer) {
        if (!b->root->connected)
            return -3;
        if (time_reached(deadline)) {
            pio_usb_host_endpoint_abort_transfer(0, REC_ADDR, REC_BULK_EP);
            return -4;
        }
        tight_loop_contents();
    }
    return ep->actual_len == len ? 0 : -5;
}

// Address, configure and find the bulk endpoint of a device in recovery mode
static int recovery_open(bus_t *b) {
    uint8_t desc[18];
    if (ctrl(b, 0x80, 6, 0x0100, 0, desc, sizeof(desc), 500)) {
        INFO("no device descriptor");
        return -1;
    }
    uint16_t vid = desc[8] | desc[9] << 8, pid = desc[10] | desc[11] << 8;
    if (vid != 0x05ac || (pid & 0xfff0) != 0x1280) {
        INFO("VID:0x%04X PID:0x%04X is not in recovery mode", vid, pid);
        return -1;
    }
    INFO("recovery mode, PID 0x%04X", pid);

    if (usb_configure(b)) {
        INFO("cannot configure the recovery device");
        return -1;
    }

    static uint8_t cfg[256];
    if (ctrl(b, 0x80, 6, 0x0200, 0, cfg, 9, 500))
        return -1;
    uint16_t total = cfg[2] | cfg[3] << 8;
    if (total > sizeof(cfg))
        total = sizeof(cfg);
    if (ctrl(b, 0x80, 6, 0x0200, 0, cfg, total, 500))
        return -1;

    // The upload endpoint is in interface 0. Interface 1 alt 1 is iBoot's
    // serial console, which nothing here needs.
    int iface = -1, alt = 0;
    for (int i = 0; i + 1 < total && cfg[i]; i += cfg[i]) {
        if (cfg[i + 1] == DESC_TYPE_INTERFACE) {
            iface = cfg[i + 2];
            alt = cfg[i + 3];
        } else if (cfg[i + 1] == DESC_TYPE_ENDPOINT && iface == 0 && alt == 0 &&
                   !pio_usb_host_endpoint_open(0, REC_ADDR, &cfg[i], false)) {
            return -1;
        }
    }
    return find_ep(REC_ADDR, REC_BULK_EP) ? 0 : -1;
}

static int recovery_send(bus_t *b, const uint8_t *data, size_t len) {
    endpoint_t *ep = find_ep(REC_ADDR, REC_BULK_EP);

    // iBoot wants this before an upload
    if (ctrl(b, 0x41, 0, 0, 0, NULL, 0, 1000))
        return -1;

    for (size_t off = 0; off < len; off += BULK_CHUNK) {
        size_t n = len - off < BULK_CHUNK ? len - off : BULK_CHUNK;
        int rc = bulk_out(b, data + off, n);
        if (rc) {
            INFO("bulk upload failed at 0x%x: %d, sent %d", off, rc, ep->actual_len);
            return -1;
        }
    }
    // a short packet ends the upload
    if (len % ep->size == 0 && bulk_out(b, NULL, 0))
        return -1;
    return 0;
}

// Reset and ask until the device answers as recovery mode
static bool wait_recovery(bus_t *b, uint32_t timeout_ms) {
    absolute_time_t deadline = make_timeout_time_ms(timeout_ms);
    for (;;) {
        if (b->root->connected) {
            bus_reset_ep0_reopen(b);
            uint8_t desc[18];
            if (!ctrl(b, 0x80, 6, 0x0100, 0, desc, sizeof(desc), 200) &&
                ((desc[10] | desc[11] << 8) & 0xfff0) == 0x1280)
                return true;
        }
        if (time_reached(deadline))
            return false;
        sleep_ms(1000);
    }
}

static int recovery_probe(bus_t *b, __unused void *ctx) {
    return wait_recovery(b, 0);
}

static int recovery_boot(bus_t *b, __unused void *ctx) {
    // iBSS brings USB back up as a recovery mode device. Its disconnect can be
    // too short to see between frames, so poll for it instead.
    if (!wait_recovery(b, 30000)) {
        INFO("iBSS did not come up in recovery mode");
        return -1;
    }

    if (recovery_open(b))
        return -1;

    for (size_t i = 0; i < count_of(images); i++) {
        size_t len = images[i].end - images[i].start;
        if (!len)
            continue;

        INFO("sending %s, %u bytes", images[i].name, len);
        if (recovery_send(b, images[i].start, len))
            return -1;

        INFO("%s", images[i].cmd);
        // Like irecovery, bootx goes as request 1. iBoot runs request 0 before
        // it finishes the transfer, and for bootx that left its USB stack busy
        // and the boot hung in iBoot's teardown. The phone leaves the bus, so
        // the status of request 1 may never come either.
        bool bootx = !strcmp(images[i].cmd, "bootx");
        if (ctrl(b, 0x40, bootx, 0, 0, images[i].cmd, strlen(images[i].cmd) + 1, 2000) &&
            !bootx) {
            INFO("command failed");
            return -1;
        }
    }
    return 0;
}

bool boot_in_recovery(void) {
    return usb_bus_execute(recovery_probe, NULL, 0) == 1;
}

// LED: fast blink while sending, slow blink when booted, off on failure
int boot_run(bool in_recovery) {
    extern volatile bool pio_usb_host_burst;
    pio_usb_host_burst = true;
    led_set_state(LED_STATE_BOOTING);

    if ((!in_recovery && usb_bus_execute(dfu_boot_ibss, NULL, 0)) ||
        usb_bus_execute(recovery_boot, NULL, 0)) {
        led_set_state(LED_STATE_ERROR);
        INFO("boot FAILED!");
        return -1;
    }

    led_set_state(LED_STATE_SUCCESS);
    INFO("booted");
    return 0;
}
