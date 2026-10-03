cmake_minimum_required(VERSION 3.17)

set(PICO_BOARD pico_w CACHE STRING "Target board (pico_w or pico for the RP2040 boards)")
set(SCREEN_BOARD lcdwiki CACHE STRING "Display module: lcdwiki or waveshare")
if(SCREEN_BOARD STREQUAL "lcdwiki")
    set(SCREEN_BOARD_DEFINE SCREEN_BOARD_LCDWIKI)
    set(SCREEN_PROGRAM_NAME "Pico W LCDWIKI GUD display")
elseif(SCREEN_BOARD STREQUAL "waveshare")
    set(SCREEN_BOARD_DEFINE SCREEN_BOARD_WAVESHARE_RESTOUCH)
    set(SCREEN_PROGRAM_NAME "Pico ResTouch 2.8 GUD display")
else()
    message(FATAL_ERROR "SCREEN_BOARD must be lcdwiki or waveshare.")
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
    ${SCREEN_BOARD_DEFINE}
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
pico_set_program_name(pico_w_screen "${SCREEN_PROGRAM_NAME}")
pico_set_program_version(pico_w_screen "0.1.0")
pico_add_extra_outputs(pico_w_screen)

set_source_files_properties(
    firmware/main.c firmware/panel.c firmware/convert.c firmware/gud_usb.c firmware/usb_descriptors.c
    PROPERTIES COMPILE_OPTIONS "-Wall;-Wextra;-Werror"
)
