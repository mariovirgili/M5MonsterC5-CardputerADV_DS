/**
 * @file display.c
 * @brief Display driver using Internal ST7789 Driver configured for ILI9341 (IDF 6.0 compatible)
 */

#include "display.h"
#include "esp_log.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h" // Contiene il driver ST7789 nativo
#include "esp_lcd_panel_ops.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include <string.h>

static const char *TAG = "DISPLAY";

static esp_lcd_panel_handle_t panel_handle = NULL;

// Small line buffer for drawing operations
#define LINE_BUFFER_SIZE (DISPLAY_WIDTH * 2)  
static uint16_t line_buffer[DISPLAY_WIDTH];

// Framebuffer for screenshot functionality (320x240)
static uint16_t framebuffer[DISPLAY_WIDTH * DISPLAY_HEIGHT];

esp_err_t display_init(void)
{
    ESP_LOGI(TAG, "Initializing Display (Via ST7789 Driver for ILI9341 compatibility)...");

    // 1. Configure backlight GPIO
    // Usa #if preprocessore per evitare errori di compilazione con shift negativo
    #if DISPLAY_PIN_BL >= 0
        gpio_config_t bk_gpio_config = {
            .mode = GPIO_MODE_OUTPUT,
            .pin_bit_mask = 1ULL << DISPLAY_PIN_BL
        };
        ESP_ERROR_CHECK(gpio_config(&bk_gpio_config));
        gpio_set_level(DISPLAY_PIN_BL, 1);
    #endif

    // 2. Configure SPI bus
    spi_bus_config_t buscfg = {
        .sclk_io_num = DISPLAY_PIN_SCLK,
        .mosi_io_num = DISPLAY_PIN_MOSI,
        .miso_io_num = -1,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t),
    };
    ESP_ERROR_CHECK(spi_bus_initialize(DISPLAY_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO));

    // 3. Configure LCD panel IO
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = DISPLAY_PIN_DC,
        .cs_gpio_num = DISPLAY_PIN_CS,
        .pclk_hz = DISPLAY_SPI_FREQ,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0,
        .trans_queue_depth = 10,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)DISPLAY_SPI_HOST, &io_config, &io_handle));

    // 4. Configure LCD panel using the Built-in ST7789 driver
    // ILI9341 is compatible with ST7789 for drawing commands (RAMWR)
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = DISPLAY_PIN_RST,
        // Nota: IDF 6.0 usa 'rgb_ele_order', vecchie versioni usavano 'rgb_endian'
        // Il driver nativo ST7789 in IDF 6.0 sa cosa usare.
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR, 
        .bits_per_pixel = 16,
    };
    
    // Usiamo il driver ST7789 che è incluso in IDF.
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io_handle, &panel_config, &panel_handle));

    // 5. Initialize panel
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
    
    // 6. Configuration Specific for ILI9341
    // ILI9341 Landscape setup:
    ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel_handle, true)); 
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel_handle, true, true));
    
    // ILI9341 non richiede inversione colori (ST7789 di solito si, quindi lo forziamo a OFF)
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_handle, false));
    
    // Gap 0,0 per display standard 320x240
    ESP_ERROR_CHECK(esp_lcd_panel_set_gap(panel_handle, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y));
    
    // Turn on display
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));

    // Clear screen to black
    display_clear(COLOR_BLACK);

    ESP_LOGI(TAG, "Display initialized successfully (%dx%d)", DISPLAY_WIDTH, DISPLAY_HEIGHT);
    return ESP_OK;
}

void display_fill_rect(int x, int y, int w, int h, uint16_t color)
{
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x >= DISPLAY_WIDTH || y >= DISPLAY_HEIGHT) return;
    if (x + w > DISPLAY_WIDTH) w = DISPLAY_WIDTH - x;
    if (y + h > DISPLAY_HEIGHT) h = DISPLAY_HEIGHT - y;
    
    if (w <= 0 || h <= 0) return;

    // Swap bytes for Big Endian (Required for most SPI Displays)
    uint16_t swapped = ((color >> 8) & 0xFF) | ((color & 0xFF) << 8);
    
    for (int i = 0; i < w; i++) {
        line_buffer[i] = swapped;
    }
    
    for (int row = y; row < y + h; row++) {
        esp_lcd_panel_draw_bitmap(panel_handle, x, row, x + w, row + 1, line_buffer);
        
        // Framebuffer update
        int row_idx = row * DISPLAY_WIDTH;
        for (int col = 0; col < w; col++) {
            framebuffer[row_idx + x + col] = color;
        }
    }
}

void display_draw_pixel(int x, int y, uint16_t color)
{
    if (x < 0 || y < 0 || x >= DISPLAY_WIDTH || y >= DISPLAY_HEIGHT) return;
    
    uint16_t swapped = ((color >> 8) & 0xFF) | ((color & 0xFF) << 8);
    esp_lcd_panel_draw_bitmap(panel_handle, x, y, x + 1, y + 1, &swapped);
    framebuffer[y * DISPLAY_WIDTH + x] = color;
}

void display_draw_hline(int x, int y, int w, uint16_t color)
{
    display_fill_rect(x, y, w, 1, color);
}

void display_draw_vline(int x, int y, int h, uint16_t color)
{
    display_fill_rect(x, y, 1, h, color);
}

void display_draw_rect(int x, int y, int w, int h, uint16_t color)
{
    display_draw_hline(x, y, w, color);
    display_draw_hline(x, y + h - 1, w, color);
    display_draw_vline(x, y, h, color);
    display_draw_vline(x + w - 1, y, h, color);
}

void display_clear(uint16_t color)
{
    display_fill_rect(0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT, color);
}

void display_set_backlight(uint8_t brightness)
{
    if (DISPLAY_PIN_BL >= 0) {
        gpio_set_level(DISPLAY_PIN_BL, brightness > 0 ? 1 : 0);
    }
}

void display_flush(void)
{
    // No-op for direct drawing mode
}

const uint16_t* display_get_framebuffer(void)
{
    return framebuffer;
}