/**
 * @file display.h
 * @brief Display Driver Header with Kconfig Switch
 */

#ifndef DISPLAY_H
#define DISPLAY_H

#include "sdkconfig.h" // Important: load menuconfig settings
#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

typedef enum {
    DISPLAY_INTERNAL = 0,
    DISPLAY_EXTERNAL = 1
} display_target_t;

// --- INTERNAL Display (Always present) ---
#define INT_DISPLAY_WIDTH       240
#define INT_DISPLAY_HEIGHT      135
#define INT_DISPLAY_OFFSET_X    40
#define INT_DISPLAY_OFFSET_Y    53
#define INT_PIN_SCLK            36
#define INT_PIN_MOSI            35
#define INT_PIN_DC              34
#define INT_PIN_CS              37
#define INT_PIN_RST             33
#define INT_PIN_BL              38
#define INT_SPI_HOST            SPI2_HOST
#define INT_SPI_FREQ            40000000

// --- EXTERNAL Display (Only if enabled in menuconfig) ---
#ifdef CONFIG_CARDPUTER_DUAL_DISPLAY
    #define EXT_DISPLAY_WIDTH       320
    #define EXT_DISPLAY_HEIGHT      240
    #define EXT_DISPLAY_OFFSET_X    0
    #define EXT_DISPLAY_OFFSET_Y    0
    #define EXT_PIN_SCLK            15
    #define EXT_PIN_MOSI            13
    #define EXT_PIN_DC              6
    #define EXT_PIN_CS              5
    #define EXT_PIN_RST             3
    #define EXT_PIN_BL              -1 
    #define EXT_SPI_HOST            SPI3_HOST
    #define EXT_SPI_FREQ            20000000 
#else
    // Fallback definitions if External is disabled
    // (Redirect to internal dimensions/pins just in case)
    #define EXT_DISPLAY_WIDTH       INT_DISPLAY_WIDTH
    #define EXT_DISPLAY_HEIGHT      INT_DISPLAY_HEIGHT
#endif

// --- MACROS (Generic API Target) ---
#ifdef CONFIG_CARDPUTER_DUAL_DISPLAY
    // Standard apps target the Big Screen
    #define DISPLAY_WIDTH           EXT_DISPLAY_WIDTH
    #define DISPLAY_HEIGHT          EXT_DISPLAY_HEIGHT
#else
    // Standard apps are forced to the Small Screen
    #define DISPLAY_WIDTH           INT_DISPLAY_WIDTH
    #define DISPLAY_HEIGHT          INT_DISPLAY_HEIGHT
#endif

// RGB565 Helper & Colors
#define RGB565(r, g, b) ((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | (((b) & 0xF8) >> 3))
#define COLOR_BLACK     0x0000
#define COLOR_WHITE     0xFFFF
#define COLOR_RED       0xF800
#define COLOR_GREEN     0x07E0
#define COLOR_BLUE      0x001F
#define COLOR_CYAN      0x07FF
#define COLOR_MAGENTA   0xF81F
#define COLOR_YELLOW    0xFFE0
#define COLOR_ORANGE    0xFD20
#define COLOR_GRAY      0x8410
#define COLOR_DARK_GRAY 0x2104

// --- Init ---
esp_err_t display_init(void);

// --- Standard API ---
void display_fill_rect(int x, int y, int w, int h, uint16_t color);
void display_draw_pixel(int x, int y, uint16_t color);
void display_draw_hline(int x, int y, int w, uint16_t color);
void display_draw_vline(int x, int y, int h, uint16_t color);
void display_draw_rect(int x, int y, int w, int h, uint16_t color);
void display_clear(uint16_t color);

// --- INTERNAL Specific API ---
void display_fill_rect_int(int x, int y, int w, int h, uint16_t color);
void display_draw_pixel_int(int x, int y, uint16_t color);
void display_clear_target(display_target_t target, uint16_t color);

// --- Utils ---
void display_set_backlight(uint8_t brightness);
void display_flush(void);
const uint16_t* display_get_framebuffer(void);

#endif // DISPLAY_H