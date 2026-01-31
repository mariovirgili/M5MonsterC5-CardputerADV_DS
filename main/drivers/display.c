/**
 * @file display.c
 * @brief Smart Display Driver (Switchable Single/Dual Mode)
 */

#include "display.h"
#include "esp_log.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include <string.h>

static const char *TAG = "DISPLAY";

// Handles
static esp_lcd_panel_handle_t panel_handle_int = NULL;
#ifdef CONFIG_CARDPUTER_DUAL_DISPLAY
static esp_lcd_panel_handle_t panel_handle_ext = NULL;
#endif

// Buffers
#define MAX_BUFFER_WIDTH 320
static uint16_t line_buffer[MAX_BUFFER_WIDTH];

// Framebuffer (Only needed for External/Screenshot logic)
#ifdef CONFIG_CARDPUTER_DUAL_DISPLAY
static uint16_t framebuffer_ext[EXT_DISPLAY_WIDTH * EXT_DISPLAY_HEIGHT];
#endif

// --- Internal Init (Always Used) ---
static esp_err_t init_internal_display(void)
{
    ESP_LOGI(TAG, "Initializing Internal Display (ST7789)...");
    
    gpio_config_t bk_gpio_config = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = 1ULL << INT_PIN_BL
    };
    ESP_ERROR_CHECK(gpio_config(&bk_gpio_config));
    gpio_set_level(INT_PIN_BL, 1);

    spi_bus_config_t buscfg = {
        .sclk_io_num = INT_PIN_SCLK,
        .mosi_io_num = INT_PIN_MOSI,
        .miso_io_num = -1,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = INT_DISPLAY_WIDTH * INT_DISPLAY_HEIGHT * sizeof(uint16_t),
    };
    ESP_ERROR_CHECK(spi_bus_initialize(INT_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO));

    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = INT_PIN_DC,
        .cs_gpio_num = INT_PIN_CS,
        .pclk_hz = INT_SPI_FREQ,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0,
        .trans_queue_depth = 10,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(INT_SPI_HOST, &io_config, &io_handle));

    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = INT_PIN_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR,
        .bits_per_pixel = 16,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io_handle, &panel_config, &panel_handle_int));

    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle_int));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle_int));
    ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel_handle_int, true));
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel_handle_int, true, false));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_handle_int, true));
    ESP_ERROR_CHECK(esp_lcd_panel_set_gap(panel_handle_int, INT_DISPLAY_OFFSET_X, INT_DISPLAY_OFFSET_Y));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle_int, true));
    
    return ESP_OK;
}

// --- External Init (Conditional) ---
#ifdef CONFIG_CARDPUTER_DUAL_DISPLAY
static esp_err_t init_external_display(void)
{
    ESP_LOGI(TAG, "Initializing External Display (ILI9341)...");

    #if EXT_PIN_BL >= 0
    gpio_config_t bk_gpio_config = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = 1ULL << EXT_PIN_BL
    };
    ESP_ERROR_CHECK(gpio_config(&bk_gpio_config));
    gpio_set_level(EXT_PIN_BL, 1);
    #endif

    spi_bus_config_t buscfg = {
        .sclk_io_num = EXT_PIN_SCLK,
        .mosi_io_num = EXT_PIN_MOSI,
        .miso_io_num = -1,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = EXT_DISPLAY_WIDTH * EXT_DISPLAY_HEIGHT * sizeof(uint16_t),
    };
    ESP_ERROR_CHECK(spi_bus_initialize(EXT_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO));

    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = EXT_PIN_DC,
        .cs_gpio_num = EXT_PIN_CS,
        .pclk_hz = EXT_SPI_FREQ,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0,
        .trans_queue_depth = 10,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(EXT_SPI_HOST, &io_config, &io_handle));

    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = EXT_PIN_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR,
        .bits_per_pixel = 16,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io_handle, &panel_config, &panel_handle_ext));

    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle_ext));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle_ext));
    ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel_handle_ext, true));
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel_handle_ext, true, true));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_handle_ext, false));
    ESP_ERROR_CHECK(esp_lcd_panel_set_gap(panel_handle_ext, EXT_DISPLAY_OFFSET_X, EXT_DISPLAY_OFFSET_Y));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle_ext, true));

    return ESP_OK;
}
#endif

// --- Main Init ---
esp_err_t display_init(void)
{
    esp_err_t ret = init_internal_display();
    if (ret != ESP_OK) return ret;

#ifdef CONFIG_CARDPUTER_DUAL_DISPLAY
    ret = init_external_display();
    if (ret != ESP_OK) return ret;
    
    display_clear_target(DISPLAY_INTERNAL, COLOR_BLACK);
    display_clear_target(DISPLAY_EXTERNAL, COLOR_BLACK);
#else
    ESP_LOGW(TAG, "Dual Display Disabled: Using Single Screen Mode");
    display_clear(COLOR_BLACK);
#endif

    return ESP_OK;
}

// --- Unified Drawing Logic ---

void display_fill_rect_target(display_target_t target, int x, int y, int w, int h, uint16_t color)
{
    esp_lcd_panel_handle_t handle;
    int max_w, max_h;

#ifdef CONFIG_CARDPUTER_DUAL_DISPLAY
    // DUAL MODE: Route to correct screen
    if (target == DISPLAY_INTERNAL) {
        handle = panel_handle_int;
        max_w = INT_DISPLAY_WIDTH;
        max_h = INT_DISPLAY_HEIGHT;
    } else {
        handle = panel_handle_ext;
        max_w = EXT_DISPLAY_WIDTH;
        max_h = EXT_DISPLAY_HEIGHT;
    }
#else
    // SINGLE MODE: Always route to Internal
    // Ignore 'target' parameter, force everything to internal screen
    handle = panel_handle_int;
    max_w = INT_DISPLAY_WIDTH;
    max_h = INT_DISPLAY_HEIGHT;
#endif

    // Clipping
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x >= max_w || y >= max_h) return;
    if (x + w > max_w) w = max_w - x;
    if (y + h > max_h) h = max_h - y;
    if (w <= 0 || h <= 0) return;

    uint16_t swapped = ((color >> 8) & 0xFF) | ((color & 0xFF) << 8);
    for (int i = 0; i < w; i++) line_buffer[i] = swapped;
    
    for (int row = y; row < y + h; row++) {
        esp_lcd_panel_draw_bitmap(handle, x, row, x + w, row + 1, line_buffer);
        
#ifdef CONFIG_CARDPUTER_DUAL_DISPLAY
        // Framebuffer update only needed for External (apps/screenshots)
        if (target == DISPLAY_EXTERNAL) {
            int row_idx = row * EXT_DISPLAY_WIDTH + x;
            for (int col = 0; col < w; col++) {
                framebuffer_ext[row_idx + col] = color;
            }
        }
#endif
    }
}

// --- Public API Implementation ---

// Standard API (Apps) -> Maps to External (in Dual) or Internal (in Single)
void display_fill_rect(int x, int y, int w, int h, uint16_t color) {
#ifdef CONFIG_CARDPUTER_DUAL_DISPLAY
    display_fill_rect_target(DISPLAY_EXTERNAL, x, y, w, h, color);
#else
    display_fill_rect_target(DISPLAY_INTERNAL, x, y, w, h, color);
#endif
}

// Internal API (Menu) -> Maps to Internal (Always)
void display_fill_rect_int(int x, int y, int w, int h, uint16_t color) {
    display_fill_rect_target(DISPLAY_INTERNAL, x, y, w, h, color);
}

void display_draw_pixel(int x, int y, uint16_t color) {
    // Simplified: reuse rect for consistency, or implement optimized per-pixel routing
    display_fill_rect(x, y, 1, 1, color);
}

void display_draw_pixel_int(int x, int y, uint16_t color) {
    display_fill_rect_int(x, y, 1, 1, color);
}

void display_draw_hline(int x, int y, int w, uint16_t color) {
    display_fill_rect(x, y, w, 1, color);
}

void display_draw_vline(int x, int y, int h, uint16_t color) {
    display_fill_rect(x, y, 1, h, color);
}

void display_draw_rect(int x, int y, int w, int h, uint16_t color) {
    display_draw_hline(x, y, w, color);
    display_draw_hline(x, y + h - 1, w, color);
    display_draw_vline(x, y, h, color);
    display_draw_vline(x + w - 1, y, h, color);
}

void display_clear(uint16_t color) {
#ifdef CONFIG_CARDPUTER_DUAL_DISPLAY
    display_fill_rect_target(DISPLAY_EXTERNAL, 0, 0, EXT_DISPLAY_WIDTH, EXT_DISPLAY_HEIGHT, color);
#else
    display_fill_rect_target(DISPLAY_INTERNAL, 0, 0, INT_DISPLAY_WIDTH, INT_DISPLAY_HEIGHT, color);
#endif
}

void display_clear_target(display_target_t target, uint16_t color) {
    int w = (target == DISPLAY_INTERNAL) ? INT_DISPLAY_WIDTH : EXT_DISPLAY_WIDTH;
    int h = (target == DISPLAY_INTERNAL) ? INT_DISPLAY_HEIGHT : EXT_DISPLAY_HEIGHT;
#ifndef CONFIG_CARDPUTER_DUAL_DISPLAY
    // Force dimensions if single screen
    w = INT_DISPLAY_WIDTH;
    h = INT_DISPLAY_HEIGHT;
#endif
    display_fill_rect_target(target, 0, 0, w, h, color);
}

void display_set_backlight(uint8_t brightness) {
    gpio_set_level(INT_PIN_BL, brightness > 0 ? 1 : 0);
}

void display_flush(void) {}

const uint16_t* display_get_framebuffer(void) {
#ifdef CONFIG_CARDPUTER_DUAL_DISPLAY
    return framebuffer_ext;
#else
    return NULL; // Not supported in single mode to save RAM
#endif
}