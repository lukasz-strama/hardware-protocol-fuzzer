# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.2.0)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.2.0-a4)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico CACHE STRING "Board type")

cmake_minimum_required(VERSION 3.13)

#set(PICO_SDK_PATH "C:/Users/Lenovo/.pico-sdk/sdk/2.2.0" CACHE PATH "Path to the Raspberry Pi Pico SDK" FORCE)
if(NOT DEFINED PICO_SDK_PATH)
    set(PICO_SDK_PATH $ENV{PICO_SDK_PATH} CACHE PATH "Path to the Raspberry Pi Pico SDK")
endif()

include(pico_sdk_import.cmake)

project(Pico_Sniffer C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(Pico_Sniffer
    firmware/main.c
    firmware/usb/usb_transport.c
    firmware/protocol/protocol.c
    firmware/protocol/protocol_handlers.c
    firmware/protocol/protocol_handler_dispatcher.c
    firmware/session/session.c
    firmware/capture/capture_i2c.c
    firmware/capture/capture_uart.c
    firmware/capture/capture_common.c
    firmware/ringbuffer/ringbuffer.c
    firmware/utils/crc16.c
    firmware/utils/timestamp.c
    firmware/capture/trace_encoder.c
    firmware/usb/usb_descriptors.c
    uart_selftest.c
)

pico_generate_pio_header(Pico_Sniffer ${CMAKE_CURRENT_LIST_DIR}/firmware/capture/capture_uart.pio)

target_include_directories(Pico_Sniffer PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/firmware
    ${CMAKE_CURRENT_LIST_DIR}/firmware/usb
    ${CMAKE_CURRENT_LIST_DIR}/firmware/protocol
    ${CMAKE_CURRENT_LIST_DIR}/firmware/session
    ${CMAKE_CURRENT_LIST_DIR}/firmware/capture
    ${CMAKE_CURRENT_LIST_DIR}/firmware/ringbuffer
    ${CMAKE_CURRENT_LIST_DIR}/firmware/utils
)

target_link_libraries(Pico_Sniffer
    pico_stdlib
    hardware_pio
    hardware_i2c
    hardware_uart
    hardware_clocks
    tinyusb_device
    tinyusb_board
    hardware_dma
)

# WAŻNE: stdio_usb=0, bo USB jest zarządzane ręcznie przez TinyUSB (tusb_config.h).
# Włączanie obu jednocześnie powoduje konflikt – oba próbują zainicjować USB hardware.
pico_enable_stdio_usb(Pico_Sniffer 0)
pico_enable_stdio_uart(Pico_Sniffer 1)   # debug na GP0/GP1

pico_add_extra_outputs(Pico_Sniffer)