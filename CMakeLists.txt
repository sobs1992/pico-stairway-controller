cmake_minimum_required(VERSION 3.13)

if(NOT DEFINED ENV{PICO_SDK_PATH})
    message(FATAL_ERROR "Please create environment variable PICO_SDK_PATH and specify path to pico_sdk.")
endif()

set(CMAKE_COMPILE_WARNING_AS_ERROR ON)

# initialize the SDK directly
include($ENV{PICO_SDK_PATH}/pico_sdk_init.cmake)

project(main)

# initialize the Raspberry Pi Pico SDK
pico_sdk_init()

add_executable(main
    main.c
    ws2812_pio/ws2812.c
    stairway/stairway_leds.c
    stairway/stairway_sensors.c
    modbus_master/modbus_master.c
    light_sensor/light_sensor.c
    settings/settings.c
    commands/commands.c
    cli/cli.c
    network/network.c
    network/dhcp_server/dhcpserver.c
    network/dns_server/dnsserver.c
    network/http_server/http_server.c
)

pico_generate_pio_header(main ${CMAKE_CURRENT_LIST_DIR}/ws2812_pio/ws2812.pio)

target_include_directories(main PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/ws2812_pio
    ${CMAKE_CURRENT_LIST_DIR}/stairway
    ${CMAKE_CURRENT_LIST_DIR}/modbus_master
    ${CMAKE_CURRENT_LIST_DIR}/light_sensor
    ${CMAKE_CURRENT_LIST_DIR}/settings
    ${CMAKE_CURRENT_LIST_DIR}/commands
    ${CMAKE_CURRENT_LIST_DIR}/cli
    ${CMAKE_CURRENT_LIST_DIR}/network
    ${CMAKE_CURRENT_LIST_DIR}/network/dhcp_server
    ${CMAKE_CURRENT_LIST_DIR}/network/dns_server
    ${CMAKE_CURRENT_LIST_DIR}/network/http_server
    ${CMAKE_CURRENT_LIST_DIR}/
)

# Add pico_stdlib library which aggregates commonly used features
target_link_libraries(
    main pico_stdlib 
    pico_cyw43_arch_lwip_poll
    hardware_pio
    hardware_clocks
    hardware_uart
    hardware_adc
    hardware_watchdog
    pico_time
)

# enable usb output, disable uart output
pico_enable_stdio_usb(main 1)
pico_enable_stdio_uart(main 0)

# You can change the address below to change the address of the access point
pico_configure_ip4_address(main PRIVATE
        CYW43_DEFAULT_IP_AP_ADDRESS 192.168.4.1
        )

# create map/bin/hex/uf2 file in addition to ELF.
pico_add_extra_outputs(main)