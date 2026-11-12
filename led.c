#include <stdio.h>
#include "pico/time.h"
#include "led.h"

#if LED_CYW43

/*
 * Pico W / Pico 2 W: the LED hangs off the CYW43 wireless chip,
 * every change is an SPI transaction done from core0 IRQs
 */

#include "cyw43.h"
#include "hardware/dma.h"
#include "hardware/pio.h"
#include "pico/async_context_threadsafe_background.h"
#include "pico/cyw43_driver.h"

// CYW43 bus stays awake (polled every 50ms) for this long after the last access
#define CYW43_BUS_SLEEP_MS  (CYW43_SLEEP_MAX * 50 + 100)

static struct {
    async_context_threadsafe_background_t ctx;
    async_at_time_worker_t blink;
    uint32_t period_ms;
    bool on;
} led_ctx = { 0 };

static void led_put(bool on) {
    led_ctx.on = on;
    cyw43_gpio_set(&cyw43_state, CYW43_WL_GPIO_LED_PIN, on);
}

static void _led_blink_cb(async_context_t *ctx, async_at_time_worker_t *w) {
    led_put(!led_ctx.on);
    async_context_add_at_time_worker_in_ms(ctx, w, led_ctx.period_ms);
}

static void led_set_blinking(uint32_t period_ms) {
    async_context_remove_at_time_worker(&led_ctx.ctx.core, &led_ctx.blink);

    led_ctx.period_ms = period_ms;
    if (period_ms) {
        async_context_add_at_time_worker_in_ms(&led_ctx.ctx.core, &led_ctx.blink, period_ms);
    }
}

void led_init(void) {
    async_context_threadsafe_background_config_t cfg = async_context_threadsafe_background_default_config();
    async_context_threadsafe_background_init(&led_ctx.ctx, &cfg);
    cyw43_driver_init(&led_ctx.ctx.core);
    led_ctx.blink.do_work = _led_blink_cb;

    // PIO USB hard-claims PIO0 SM0-2 and DMA ch0 later on,
    // so hold them while the first access brings CYW43 up
    // and lets it claim its own PIO SM & DMA channels
    pio_claim_sm_mask(pio0, 0xF);
    dma_channel_claim(0);

    led_put(false);

    for (uint sm = 0; sm < 4; sm++) {
        pio_sm_unclaim(pio0, sm);
    }
    dma_channel_unclaim(0);
}

void led_set_state(int state) {
    switch (state) {
        case LED_STATE_BOOTING: {
            led_set_blinking(200);
            break;
        }

        case LED_STATE_IDLE: {
            led_set_blinking(0);
            led_put(true);

            // only hit once per boot.
            // let the CYW43 bus go to sleep so its background
            // polls can't overlap the (racy) exploit run
            sleep_ms(CYW43_BUS_SLEEP_MS);
            break;
        }

        case LED_STATE_SUCCESS: {
            led_set_blinking(500);
            break;
        }

        case LED_STATE_ERROR: {
            led_set_blinking(0);
            led_put(false);
            break;
        }
    }
}

#else

void led_init(void) {
}

void led_set_state(int state) {
}

#endif
