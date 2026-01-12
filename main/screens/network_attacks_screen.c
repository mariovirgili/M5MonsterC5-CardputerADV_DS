/**
 * @file network_attacks_screen.c
 * @brief Network attacks menu screen implementation
 */

#include "network_attacks_screen.h"
#include "wifi_connect_screen.h"
#include "arp_hosts_screen.h"
#include "uart_handler.h"
#include "text_ui.h"
#include "esp_log.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "NET_ATTACKS";

// Menu indices
#define MENU_WIFI      0
#define MENU_ARP       1
#define MENU_ITEM_COUNT 2

// Screen user data
typedef struct {
    int selected_index;
} network_attacks_data_t;

static const char* get_wifi_menu_text(void)
{
    return uart_is_wifi_connected() ? "Disconnect from WiFi" : "Connect to WiFi";
}

static void draw_screen(screen_t *self)
{
    network_attacks_data_t *data = (network_attacks_data_t *)self->user_data;
    
    ui_clear();
    
    // Draw title
    ui_draw_title("Network Attacks");
    
    // Draw menu items
    ui_draw_menu_item(1, get_wifi_menu_text(), data->selected_index == MENU_WIFI, false, false);
    ui_draw_menu_item(2, "ARP Poisoning", data->selected_index == MENU_ARP, false, false);
    
    // Draw status bar
    ui_draw_status("UP/DOWN:Navigate ENTER:Select ESC:Back");
}

// Optimized: redraw only two changed rows
static void redraw_selection(network_attacks_data_t *data, int old_index, int new_index)
{
    const char *old_text = (old_index == MENU_WIFI) ? get_wifi_menu_text() : "ARP Poisoning";
    const char *new_text = (new_index == MENU_WIFI) ? get_wifi_menu_text() : "ARP Poisoning";
    
    ui_draw_menu_item(old_index + 1, old_text, false, false, false);
    ui_draw_menu_item(new_index + 1, new_text, true, false, false);
}

static void on_key(screen_t *self, key_code_t key)
{
    network_attacks_data_t *data = (network_attacks_data_t *)self->user_data;
    
    switch (key) {
        case KEY_UP:
            if (data->selected_index > 0) {
                int old = data->selected_index;
                data->selected_index--;
                redraw_selection(data, old, data->selected_index);
            }
            break;
            
        case KEY_DOWN:
            if (data->selected_index < MENU_ITEM_COUNT - 1) {
                int old = data->selected_index;
                data->selected_index++;
                redraw_selection(data, old, data->selected_index);
            }
            break;
            
        case KEY_ENTER:
        case KEY_SPACE:
            if (data->selected_index == MENU_WIFI) {
                if (uart_is_wifi_connected()) {
                    // Disconnect
                    uart_send_command("wifi_disconnect");
                    uart_set_wifi_connected(false);
                    // Redraw to update menu text
                    draw_screen(self);
                } else {
                    // Connect - push WiFi connect screen
                    screen_manager_push(wifi_connect_screen_create, NULL);
                }
            } else if (data->selected_index == MENU_ARP) {
                // ARP Poisoning - push hosts screen
                screen_manager_push(arp_hosts_screen_create, NULL);
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
    if (self->user_data) {
        free(self->user_data);
    }
}

static void on_resume(screen_t *self)
{
    // Redraw to update WiFi connection status text
    draw_screen(self);
}

screen_t* network_attacks_screen_create(void *params)
{
    (void)params;
    
    ESP_LOGI(TAG, "Creating network attacks screen...");
    
    screen_t *screen = screen_alloc();
    if (!screen) return NULL;
    
    // Allocate user data
    network_attacks_data_t *data = calloc(1, sizeof(network_attacks_data_t));
    if (!data) {
        free(screen);
        return NULL;
    }
    
    screen->user_data = data;
    screen->on_key = on_key;
    screen->on_destroy = on_destroy;
    screen->on_resume = on_resume;
    screen->on_draw = draw_screen;
    
    // Draw initial screen
    draw_screen(screen);
    
    ESP_LOGI(TAG, "Network attacks screen created");
    return screen;
}
