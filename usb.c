#include <stdint.h>

#include "pico/multicore.h"

#include "usb.h"
#include "bus.h"
#include "log.h"

static struct {
    usb_executee_t func;
    void *ctx;
} usb_exec_ctx;

static bus_t gBus = { 0 };

// core1 owns the PIO USB bus and runs whatever core0 hands it
static void usb_task(void) {
    while (1) {
        multicore_fifo_pop_blocking();
        multicore_fifo_push_blocking(usb_exec_ctx.func(&gBus, usb_exec_ctx.ctx));
    }
}

void usb_start(void) {
    multicore_reset_core1();
    multicore_launch_core1(usb_task);
}

int usb_bus_execute(usb_executee_t func, void *ctx, uint64_t timeout) {
    usb_exec_ctx.func = func;
    usb_exec_ctx.ctx = ctx;
    multicore_fifo_push_blocking(0);

    uint32_t out = -1;

    if (timeout) {
        if (!multicore_fifo_pop_timeout_us(timeout, &out)) {
            INFO("TIMEOUT");
        }
    } else {
        out = multicore_fifo_pop_blocking();
    }

    return out;
}

#define DEFAULT_TIMEOUT_US  (100 * 1000)

static int init_cb(bus_t *b, __unused void *ctx) {
    bus_init(b);
    return 0;
}

static int wait_cb(bus_t *b, __unused void *ctx) {
    bus_wait_for_connect(b);
    return 0;
}

static int reset_cb(bus_t *b, __unused void *ctx) {
    bus_reset_ep0_reopen(b);
    return 0;
}

int usb_bus_init(void) {
    return usb_bus_execute(init_cb, NULL, DEFAULT_TIMEOUT_US);
}

int usb_bus_wait_for_device(void) {
    INFO("waiting for device...");

    int ret = usb_bus_execute(wait_cb, NULL, 0);
    if (ret == 0) {
        INFO("connected, speed = %s", gBus.root->is_fullspeed ? "FS" : "LS");
    }

    return ret;
}

int usb_bus_reset_open_ep0(void) {
    int ret = usb_bus_execute(reset_cb, NULL, DEFAULT_TIMEOUT_US);
    if (ret == 0) {
        INFO("opened EP0");
    }

    return ret;
}
