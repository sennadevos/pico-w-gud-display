#ifndef SCREEN_BOARD_H
#define SCREEN_BOARD_H

/* Display module selection. Exactly one of these is compiled in; the LCDWIKI
 * MSP3222/MSP3223 is the default so existing builds are unchanged.
 *
 *   -DSCREEN_BOARD=lcdwiki    Pico W + LCDWIKI MSP3222/MSP3223 (hand-wired)
 *   -DSCREEN_BOARD=waveshare  Pico + Waveshare Pico-ResTouch-LCD-2.8 (hat)
 */

#if defined(SCREEN_BOARD_WAVESHARE_RESTOUCH)

/* Waveshare Pico-ResTouch-LCD-2.8. Crowned onto the Pico's 40-pin header, so
 * the pins are fixed. Verified against Waveshare's schematic (2021-04-28) and
 * their C demo (LCD_Driver.c, LCD_2_8 branch).
 *
 * Panel:  ST7789VW, 240x320 native, module sold as 320x240 landscape (the
 *         active area is 57.6 x 43.2 mm). IPS.
 * Touch:  XPT2046 on the same SPI bus with its own CS/IRQ - not implemented.
 * SD:     microSD on GP5/GP18..GP22 - not implemented, leave it empty.
 * Power:  VCC is fed from VSYS (5 V); an RT9193-33 LDO makes the 3.3 V rail.
 * Light:  LCD_BL -> 1k -> NPN base (8050), LEDs from 3V3, active high.
 */
#define SCREEN_PANEL_ST7789 1
#define SCREEN_SPI_INDEX 1 /* SCK/MOSI/MISO on GP10/11/12 are SPI1 pins */
#define SCREEN_PIN_DC 8
#define SCREEN_PIN_CS 9
#define SCREEN_PIN_SCK 10
#define SCREEN_PIN_MOSI 11
#define SCREEN_PIN_MISO 12
#define SCREEN_PIN_BACKLIGHT 13
#define SCREEN_PIN_RESET 15
#define SCREEN_BASE_WIDTH 320 /* landscape; rotate with -DSCREEN_ROTATION=90 */
#define SCREEN_BASE_HEIGHT 240
#define SCREEN_BASE_WIDTH_MM 58
#define SCREEN_BASE_HEIGHT_MM 43
#define SCREEN_EDID_NAME "Pico-RT28"
#define SCREEN_USB_PRODUCT "Pico ResTouch 2.8 Display"
#define SCREEN_DESCRIPTION "RP2040 Pico, Waveshare Pico-ResTouch-LCD-2.8, native Linux GUD"

#elif defined(SCREEN_BOARD_LCDWIKI)

/* LCDWIKI MSP3222/MSP3223 3.2" IPS panel (ILI9341V) on a Pico W, hand-wired
 * as documented in the README. */
#define SCREEN_PANEL_ILI9341 1
#define SCREEN_SPI_INDEX 0 /* GP16..GP19 are SPI0 pins */
#define SCREEN_PIN_MISO 16
#define SCREEN_PIN_CS 17
#define SCREEN_PIN_SCK 18
#define SCREEN_PIN_MOSI 19
#define SCREEN_PIN_DC 20
#define SCREEN_PIN_RESET 21
#define SCREEN_PIN_BACKLIGHT 22
#define SCREEN_BASE_WIDTH 240 /* portrait */
#define SCREEN_BASE_HEIGHT 320
#define SCREEN_BASE_WIDTH_MM 49
#define SCREEN_BASE_HEIGHT_MM 65
#define SCREEN_EDID_NAME "PicoW-TFT"
#define SCREEN_USB_PRODUCT "Pico W LCDWIKI Display"
#define SCREEN_DESCRIPTION "RP2040 Pico W, LCDWIKI MSP3222/MSP3223, native Linux GUD"

#else
#error "Select a board: -DSCREEN_BOARD=lcdwiki or -DSCREEN_BOARD=waveshare"
#endif

#endif
