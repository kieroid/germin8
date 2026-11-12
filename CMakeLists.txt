cmake_minimum_required(VERSION 3.13)

# ====================================================================================

set(PICO_BOARD pico2_w)

include(pico_sdk_import.cmake)

project(germin8 C CXX ASM)

set(CMAKE_BUILD_TYPE MinSizeRel)

add_compile_definitions(
    BOARD_NAME="Raspberry Pi Pico 2 W"
    PIO_USB_DP_PIN_DEFAULT=12
    # on-board LED via the CYW43 wireless chip, set to 0 if it hurts reliability
    LED_CYW43=1
    CYW43_LWIP=0
)

pico_sdk_init()

# The phone's boot images go into flash. They are Apple's, so they come from a
# badapple checkout instead of this repository: its build/iboot after
# "make images" and "make bootbin".
set(BADAPPLE_IBOOT "" CACHE PATH "badapple's build/iboot")
if(NOT EXISTS "${BADAPPLE_IBOOT}/kernelcache-chainload.img4")
    message(FATAL_ERROR "set BADAPPLE_IBOOT to badapple's build/iboot, after make bootbin")
endif()
set(IMG_IBSS "${BADAPPLE_IBOOT}/iBSS.patched.raw")
set(IMG_DEVICETREE "${BADAPPLE_IBOOT}/devicetree.img4")
set(IMG_TRUSTCACHE "${BADAPPLE_IBOOT}/trustcache.img4")
set(IMG_KERNELCACHE "${BADAPPLE_IBOOT}/kernelcache-chainload.img4" CACHE FILEPATH "the m1n1 kernelcache to send")
# GPU firmware, optional like in badapple
if(EXISTS "${BADAPPLE_IBOOT}/gfx.img4")
    set(IMG_GFX "${BADAPPLE_IBOOT}/gfx.img4")
endif()
configure_file(images.S.in images.S @ONLY)
set_property(SOURCE ${CMAKE_CURRENT_BINARY_DIR}/images.S APPEND PROPERTY OBJECT_DEPENDS
    ${IMG_IBSS} ${IMG_DEVICETREE} ${IMG_TRUSTCACHE} ${IMG_KERNELCACHE} ${IMG_GFX})

add_executable(germin8
    main.c
    led.c
    bus.c
    usb.c
    exploit.c
    boot.c
    ${CMAKE_CURRENT_BINARY_DIR}/images.S
)

pico_set_program_name(germin8 "germin8")
pico_set_program_version(germin8 "1.0")

target_compile_definitions(germin8 PRIVATE
    PICO_STACK_SIZE=0x1000
    PICO_PRINTF_SUPPORT_EXPONENTIAL=0
    PICO_PRINTF_SUPPORT_FLOAT=0
    PICO_PRINTF_SUPPORT_PTRDIFF_T=0
)

pico_enable_stdio_uart(germin8 0)
pico_enable_stdio_usb(germin8 1)

add_subdirectory(pio_usb)

target_link_libraries(germin8
    pico_stdlib
    pico_cyw43_driver
    cyw43_driver_picow
    pico_async_context_threadsafe_background
    pio_usb
)

pico_add_extra_outputs(germin8)
