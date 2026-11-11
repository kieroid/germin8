cmake_minimum_required(VERSION 3.13)

# ====================================================================================

set(PICO_BOARD pico2_w)

include(pico_sdk_import.cmake)

project(usbliter8 C CXX ASM)

set(CMAKE_BUILD_TYPE MinSizeRel)

add_compile_definitions(
    BOARD_NAME="Raspberry Pi Pico 2 W"
    PIO_USB_DP_PIN_DEFAULT=12
    # on-board LED via the CYW43 wireless chip, set to 0 if it hurts reliability
    LED_CYW43=1
    CYW43_LWIP=0
)

pico_sdk_init()

add_executable(usbliter8
    main.c
    led.c
    bus.c
    usb.c
    exploit.c
)

pico_set_program_name(usbliter8 "usbliter8")
pico_set_program_version(usbliter8 "1.0")

target_compile_definitions(usbliter8 PRIVATE
    PICO_STACK_SIZE=0x1000
    PICO_PRINTF_SUPPORT_EXPONENTIAL=0
    PICO_PRINTF_SUPPORT_FLOAT=0
    PICO_PRINTF_SUPPORT_PTRDIFF_T=0
)

pico_enable_stdio_uart(usbliter8 0)
pico_enable_stdio_usb(usbliter8 1)

add_subdirectory(pio_usb)

target_link_libraries(usbliter8
    pico_stdlib
    pico_cyw43_driver
    cyw43_driver_picow
    pico_async_context_threadsafe_background
    pio_usb
)

pico_add_extra_outputs(usbliter8)
