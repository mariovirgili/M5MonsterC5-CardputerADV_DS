/**
 * @file sniffer_results_screen.c
 * @brief Sniffer results screen showing SSIDs and clients
 */

#include "sniffer_results_screen.h"
#include "uart_handler.h"
#include "text_ui.h"
#include "esp_log.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

static const char *TAG = "SNIFF_RES";

// Maximum entries
#define MAX_LINES       32
#define MAX_LINE_LEN    32

// Screen user data
typedef struct {
    char lines[MAX_LINES][MAX_LINE_LEN];
    int line_count;
    int scroll_offset;
    bool loading;
    bool needs_redraw;
    screen_t *self;
} sniffer_results_data_t;

// Forward declaration
static void draw_screen(screen_t *self);

/**
 * @brief Check if line is an ESP log line (starts with I/W/E/D followed by space and parenthesis)
 */
static bool is_esp_log_line(const char *line)
{
    if (strlen(line) < 3) return false;
    
    // Check for ESP log format: "I (", "W (", "E (", "D ("
    if ((line[0] == 'I' || line[0] == 'W' || line[0] == 'E' || line[0] == 'D') 
        && line[1] == ' ' && line[2] == '(') {
        return true;
    }
    
    // Check for other common log patterns
    if (strstr(line, "[MEM]") != NULL) return true;
    if (strncmp(line, "show_sniffer", 12) == 0) return true;  // Echo of command
    
    return false;
}

/**
 * @brief Check if line looks like a MAC address (may have leading space)
 * Format: " XX:XX:XX:XX:XX:XX" or "XX:XX:XX:XX:XX:XX"
 */
static bool is_mac_line(const char *line)
{
    const char *p = line;
    
    // Skip leading whitespace
    while (*p == ' ') p++;
    
    // Need at least 17 chars for MAC
    if (strlen(p) < 17) return false;
    
    // Check pattern: hex:hex:hex:hex:hex:hex
    for (int i = 0; i < 17; i++) {
        if (i == 2 || i == 5 || i == 8 || i == 11 || i == 14) {
            if (p[i] != ':') return false;
        } else {
            if (!isxdigit((unsigned char)p[i])) return false;
        }
    }
    return true;
}

/**
 * @brief Check if line is an SSID line (contains ", CH")
 */
static bool is_ssid_line(const char *line)
{
    return strstr(line, ", CH") != NULL;
}

/**
 * @brief Check if line indicates no results
 */
static bool is_no_results_line(const char *line)
{
    return strstr(line, "No APs") != NULL || strstr(line, "no clients") != NULL;
}

/**
 * @brief UART line callback for parsing results
 */
static void uart_line_callback(const char *line, void *user_data)
{
    sniffer_results_data_t *data = (sniffer_results_data_t *)user_data;
    if (!data || data->line_count >= MAX_LINES) return;
    
    // Skip empty lines
    if (strlen(line) == 0) return;
    
    // Skip ESP log lines
    if (is_esp_log_line(line)) return;
    
    // Check for "no results" message
    if (is_no_results_line(line)) {
        data->loading = false;
        data->needs_redraw = true;
        return;
    }
    
    // Check if it's an SSID line or MAC line
    if (is_ssid_line(line) || is_mac_line(line)) {
        // Store line as-is (MACs already have indent from UART)
        strncpy(data->lines[data->line_count], line, MAX_LINE_LEN - 1);
        data->lines[data->line_count][MAX_LINE_LEN - 1] = '\0';
        data->line_count++;
        data->loading = false;
        data->needs_redraw = true;
    }
    // Ignore all other lines
}

static void draw_screen(screen_t *self)
{
    sniffer_results_data_t *data = (sniffer_results_data_t *)self->user_data;
    
    ui_clear();
    
    // Draw title
    ui_draw_title("Sniffer Results");
    
    if (data->loading) {
        ui_print_center(3, "Loading...", UI_COLOR_DIMMED);
    } else if (data->line_count == 0) {
        ui_print_center(3, "No results", UI_COLOR_DIMMED);
    } else {
        // Draw visible lines
        int visible_rows = 5;
        int start_row = 1;
        
        for (int i = 0; i < visible_rows; i++) {
            int line_idx = data->scroll_offset + i;
            
            if (line_idx < data->line_count) {
                const char *line = data->lines[line_idx];
                uint16_t color = UI_COLOR_TEXT;
                
                // Indented lines (MACs) are dimmed
                if (line[0] == ' ') {
                    color = UI_COLOR_DIMMED;
                } else {
                    color = UI_COLOR_HIGHLIGHT;
                }
                
                // Truncate for display
                char display[31];
                strncpy(display, line, 30);
                display[30] = '\0';
                
                ui_print(0, start_row + i, display, color);
            }
        }
        
        // Scroll indicators
        if (data->scroll_offset > 0) {
            ui_print(UI_COLS - 2, 1, "^", UI_COLOR_DIMMED);
        }
        if (data->scroll_offset + visible_rows < data->line_count) {
            ui_print(UI_COLS - 2, visible_rows, "v", UI_COLOR_DIMMED);
        }
    }
    
    // Draw status bar
    ui_draw_status("UP/DOWN:Scroll ESC:Back");
}

static void on_tick(screen_t *self)
{
    sniffer_results_data_t *data = (sniffer_results_data_t *)self->user_data;
    
    // Check if redraw needed from UART callback (thread-safe)
    if (data->needs_redraw) {
        data->needs_redraw = false;
        draw_screen(self);
    }
}

static void on_key(screen_t *self, key_code_t key)
{
    sniffer_results_data_t *data = (sniffer_results_data_t *)self->user_data;
    int visible_rows = 5;
    
    // Also check on key press for faster response
    on_tick(self);
    
    switch (key) {
        case KEY_UP:
            if (data->scroll_offset > 0) {
                data->scroll_offset--;
                draw_screen(self);
            }
            break;
            
        case KEY_DOWN:
            if (data->scroll_offset + visible_rows < data->line_count) {
                data->scroll_offset++;
                draw_screen(self);
            }
            break;
            
        case KEY_ESC:
        case KEY_Q:
        case KEY_BACKSPACE:
            screen_manager_pop();
            break;
            
        default:
            break;
    }
}

static void on_destroy(screen_t *self)
{
    sniffer_results_data_t *data = (sniffer_results_data_t *)self->user_data;
    
    // Clear UART callback
    uart_clear_line_callback();
    
    if (data) {
        free(data);
    }
}

screen_t* sniffer_results_screen_create(void *params)
{
    (void)params;  // Not used
    
    ESP_LOGI(TAG, "Creating sniffer results screen...");
    
    screen_t *screen = screen_alloc();
    if (!screen) {
        return NULL;
    }
    
    // Allocate user data
    sniffer_results_data_t *data = calloc(1, sizeof(sniffer_results_data_t));
    if (!data) {
        free(screen);
        return NULL;
    }
    
    data->loading = true;
    data->self = screen;
    
    screen->user_data = data;
    screen->on_key = on_key;
    screen->on_destroy = on_destroy;
    screen->on_draw = draw_screen;
    screen->on_tick = on_tick;
    
    // Register UART callback
    uart_register_line_callback(uart_line_callback, data);
    
    // Send command to get results
    uart_send_command("show_sniffer_results");
    
    // Draw initial screen
    draw_screen(screen);
    
    ESP_LOGI(TAG, "Sniffer results screen created");
    return screen;
}

