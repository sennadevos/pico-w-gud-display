cmake_minimum_required(VERSION 3.17)

set(PICO_BOARD pico_w CACHE STRING "Target board")
if(NOT PICO_BOARD STREQUAL "pico_w")
    message(FATAL_ERROR "This project targets the original RP2040 Pico W, not Pico 2 W.")
endif()
if(NOT PICO_SDK_PATH)
    set(PICO_SDK_PATH "${CMAKE_CURRENT_LIST_DIR}/.deps/pico-sdk")
endif()
include("${PICO_SDK_PATH}/external/pico_sdk_import.cmake")

project(pico_w_screen VERSION 0.1.0 LANGUAGES C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
pico_sdk_init()

set(SCREEN_ROTATION 0 CACHE STRING "Panel orientation: 0, 90, 180, 270")
# The ILI9341's specified maximum is 10 MHz; higher values are overclocking.
# The RP2040 rounds the request down to the next feasible divider (clk_peri/2n),
# so the ladder is 62.5, 31.25, 20.83, 15.625, 12.5, 10.42, 8.93 MHz. 62.5 MHz is
# the RP2040's SPI ceiling and was verified clean on this module and wiring.
set(SCREEN_SPI_HZ 62500000 CACHE STRING "Requested SPI clock in Hz (rounded down by the RP2040 divider)")
if(NOT SCREEN_ROTATION MATCHES "^(0|90|180|270)$")
    message(FATAL_ERROR "SCREEN_ROTATION must be 0, 90, 180 or 270.")
endif()
if(SCREEN_SPI_HZ LESS 100000 OR SCREEN_SPI_HZ GREATER 62500000)
    message(FATAL_ERROR "SCREEN_SPI_HZ must be between 100000 and 62500000.")
endif()
if(SCREEN_SPI_HZ GREATER 10000000)
    message(WARNING "SCREEN_SPI_HZ=${SCREEN_SPI_HZ} exceeds the controller's specified 10 MHz; validate the panel image.")
endif()

add_executable(pico_w_screen
    firmware/main.c
    firmware/panel.c
    firmware/convert.c
    firmware/gud_usb.c
    firmware/usb_descriptors.c
    third_party/gud/gud.c
    third_party/gud/lz4.c
)
target_include_directories(pico_w_screen PRIVATE firmware third_party/gud)
target_compile_definitions(pico_w_screen PRIVATE
    SCREEN_ROTATION=${SCREEN_ROTATION}
    SCREEN_SPI_HZ=${SCREEN_SPI_HZ}
    LZ4_FORCE_MEMORY_ACCESS=0
    LZ4_HEAPMODE=0
)
target_link_libraries(pico_w_screen PRIVATE
    pico_stdlib pico_unique_id tinyusb_device
    hardware_dma hardware_spi hardware_pwm
)
pico_enable_stdio_uart(pico_w_screen 0)
pico_enable_stdio_usb(pico_w_screen 0)
pico_set_program_name(pico_w_screen "Pico W LCDWIKI GUD display")
pico_set_program_version(pico_w_screen "0.1.0")
pico_add_extra_outputs(pico_w_screen)

set_source_files_properties(
    firmware/main.c firmware/panel.c firmware/convert.c firmware/gud_usb.c firmware/usb_descriptors.c
    PROPERTIES COMPILE_OPTIONS "-Wall;-Wextra;-Werror"
)
