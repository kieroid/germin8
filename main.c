#include <stdio.h>

#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "hardware/structs/ioqspi.h"
#include "hardware/structs/sio.h"
#include "hardware/sync.h"
#include "hardware/watchdog.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"

#include "boot.h"
#include "exploit.h"
#include "led.h"
#include "usb.h"

#define CONNECTION_FAIL_SLEEP_MS    (500)

/*
 * BOOTSEL shares the QSPI flash CS line, so it can only be sampled
 * while nothing executes from flash (pico-examples/picoboard/button)
 */
static bool __no_inline_not_in_flash_func(bootsel_pressed)(void) {
    const uint CS_PIN_INDEX = 1;
    uint32_t flags = save_and_disable_interrupts();

    hw_write_masked(&ioqspi_hw->io[CS_PIN_INDEX].ctrl,
                    GPIO_OVERRIDE_LOW << IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_LSB,
                    IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_BITS);

    for (volatile int i = 0; i < 1000; ++i) {}

    bool pressed = !(sio_hw->gpio_hi_in & SIO_GPIO_HI_IN_QSPI_CSN_BITS);

    hw_write_masked(&ioqspi_hw->io[CS_PIN_INDEX].ctrl,
                    GPIO_OVERRIDE_NORMAL << IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_LSB,
                    IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_BITS);

    restore_interrupts(flags);

    return pressed;
}

/*
 * Rebooting gives the next run the exact same fresh state
 * (and boot delay) as a power cycle, without touching exploit timings
 */
__attribute__((noreturn))
void reboot_on_bootsel(void) {
    // core1 (PIO USB SOF timer) runs from flash, which BOOTSEL sampling blocks
    multicore_reset_core1();

    printf("press BOOTSEL to run again\n");

    while (!bootsel_pressed()) {
        sleep_ms(20);
    }

    // rebooting with BOOTSEL held would land in the USB bootloader
    while (bootsel_pressed()) {
        sleep_ms(20);
    }

    watchdog_reboot(0, SRAM_END, 1);
    while (1) {}
}

__attribute__((noreturn))
void do_auto(void) {
    while (true) {
        int ret = exploit_run();

        /*
         * This case means not even device descriptor
         * could be queried
         *
         * On those boards with redundant resistor
         * it could mean that device was not even
         * connected yet
         *
         * So we sleep a bit, reset the bus and try again
         *
         */
        if (ret == -2) {
            sleep_ms(CONNECTION_FAIL_SLEEP_MS);
            usb_bus_reset_open_ep0();
            continue;
        }

        /* iBSS already up, from a run that stopped after it */
        if (ret < 0 && boot_in_recovery()) {
            printf("\n%s\n", boot_run(true) ? "boot failed" : "success");
            reboot_on_bootsel();
        }

        /* no-go failure, bailing */
        if (ret < 0) {
            led_set_state(LED_STATE_ERROR);
            printf("\nfatal failure\n");
            reboot_on_bootsel();
        }

        /* it all went well then */
        break;
    }

    // pwned now or already, so boot it
    printf("\n%s\n", boot_run(false) ? "boot failed" : "success");
    reboot_on_bootsel();
}

int main(void) {
    // PIO USB needs sys_clk to be a multiple of 12 MHz
    set_sys_clock_khz(156000, true);

    led_init();
    led_set_state(LED_STATE_BOOTING);

    stdio_init_all();

    // this delay being long enough, seems to have
    // a HUGE impact on the exploit reliability
    sleep_ms(2000);

    led_set_state(LED_STATE_IDLE);

    printf("\n============ %s v%s ============\n", PICO_PROGRAM_NAME, PICO_PROGRAM_VERSION_STRING);
    printf("built for %s, PIO USB @ GP%d/%d (D+/D-)\n\n", BOARD_NAME, PIO_USB_DP_PIN_DEFAULT, PIO_USB_DP_PIN_DEFAULT + 1);

    usb_start();
    usb_bus_init();
    usb_bus_wait_for_device();
    usb_bus_reset_open_ep0();

    do_auto();
}
