cmake_minimum_required(VERSION 3.13)

if(NOT DEFINED ENV{PICO_SDK_PATH})
    message(FATAL_ERROR "Please create environment variable PICO_SDK_PATH and specify path to pico_sdk.")
endif()

# initialize the SDK directly
include($ENV{PICO_SDK_PATH}/pico_sdk_init.cmake)

project(main)

# initialize the Raspberry Pi Pico SDK
pico_sdk_init()

add_executable(main
    main.c
    ws2812_pio/ws2812.c
    stairway/stairway_leds.c
)

pico_generate_pio_header(main ${CMAKE_CURRENT_LIST_DIR}/ws2812_pio/ws2812.pio)

target_include_directories(main PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/ws2812_pio
    ${CMAKE_CURRENT_LIST_DIR}/stairway
    ${CMAKE_CURRENT_LIST_DIR}/
)

# Add pico_stdlib library which aggregates commonly used features
target_link_libraries(
    main pico_stdlib 
    pico_cyw43_arch_none
    hardware_pio
    hardware_clocks
    pico_time
)

# enable usb output, disable uart output
pico_enable_stdio_usb(main 1)
pico_enable_stdio_uart(main 0)

# create map/bin/hex/uf2 file in addition to ELF.
pico_add_extra_outputs(main)