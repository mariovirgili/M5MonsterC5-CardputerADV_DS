/**
 * @file screenshot.c
 * @brief SD Card Driver.
 * FIX: Removed double SPI initialization to prevent crashes.
 */
#include "screenshot.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdspi_host.h"
#include "display.h"
#include <sys/stat.h>
#include <dirent.h>

// PIN CONFIG (Internal Cardputer SD)
#define SD_PIN_MISO 39
#define SD_PIN_MOSI 14
#define SD_PIN_CLK  40
#define SD_PIN_CS   12 
#define SD_SPI_HOST SPI3_HOST 

#define MOUNT_POINT "/sdcard"
#define SCREENS_DIR "/sdcard/screens"

static const char *TAG = "SCREENSHOT";
static bool sd_mounted = false;
static int screenshot_counter = 1;
static sdmmc_card_t *card = NULL;

#pragma pack(push, 1)
typedef struct { uint16_t type; uint32_t size; uint16_t r1; uint16_t r2; uint32_t offset; } bmp_fh;
typedef struct { uint32_t size; int32_t w; int32_t h; uint16_t p; uint16_t b; uint32_t c; uint32_t is; int32_t xr; int32_t yr; uint32_t nc; uint32_t ic; } bmp_ih;
#pragma pack(pop)

static void find_next_screenshot_number(void) {
    DIR *dir = opendir(SCREENS_DIR);
    if (dir == NULL) { screenshot_counter = 1; return; }
    int max_num = 0; struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        int num; if (sscanf(entry->d_name, "scr_%d.bmp", &num) == 1) { if (num > max_num) max_num = num; }
    }
    closedir(dir);
    screenshot_counter = max_num + 1;
}

esp_err_t screenshot_init(void) {
    ESP_LOGI(TAG, "Mounting SD Card...");

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = SD_PIN_CS;
    slot_config.host_id = SD_SPI_HOST;

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = SD_SPI_HOST;
    // We DO NOT initialize the bus here anymore. 
    // It is assumed initialized by display.c or main.c

    esp_err_t ret = esp_vfs_fat_sdspi_mount(MOUNT_POINT, &host, &slot_config, &mount_config, &card);
    
    if (ret == ESP_OK) {
        sd_mounted = true;
        // Create directory if it doesn't exist
        struct stat st = {0};
        if (stat(SCREENS_DIR, &st) == -1) {
            mkdir(SCREENS_DIR, 0755);
        }
        find_next_screenshot_number();
        ESP_LOGI(TAG, "SD Mounted Successfully");
        return ESP_OK;
    } else {
        ESP_LOGW(TAG, "Failed to mount SD: %s (This is normal if SD is missing or pins conflict)", esp_err_to_name(ret));
        return ret;
    }
}

bool screenshot_is_available(void) { return sd_mounted; }

static inline void rgb565_to_rgb888(uint16_t c, uint8_t *r, uint8_t *g, uint8_t *b) { 
    *r = ((c >> 11) & 0x1F) << 3; 
    *g = ((c >> 5) & 0x3F) << 2; 
    *b = (c & 0x1F) << 3; 
}

esp_err_t screenshot_take(void) {
    if(!sd_mounted) return ESP_FAIL;
    char fname[64]; snprintf(fname, 64, "%s/scr_%d.bmp", SCREENS_DIR, screenshot_counter++);
    FILE* f=fopen(fname, "wb"); if(!f) return ESP_FAIL;
    
    const uint16_t* fb = display_get_framebuffer();
    if (!fb) { fclose(f); return ESP_FAIL; }

    int w=320, h=240, rs=((w*3+3)/4)*4;
    bmp_fh fh={0x4D42, 54+rs*h, 0,0, 54}; 
    bmp_ih ih={40, w, -h, 1, 24, 0, rs*h, 0,0,0,0};
    
    fwrite(&fh, 14, 1, f); 
    fwrite(&ih, 40, 1, f);
    
    uint8_t* buf=malloc(rs);
    if (!buf) { fclose(f); return ESP_ERR_NO_MEM; }

    for(int y=0; y<h; y++) {
        memset(buf,0,rs);
        for(int x=0; x<w; x++) {
            uint16_t p=fb[y*w+x]; 
            uint8_t r,g,b; 
            rgb565_to_rgb888(p, &r, &g, &b);
            buf[x*3]=b; buf[x*3+1]=g; buf[x*3+2]=r;
        }
        fwrite(buf, rs, 1, f);
    }
    free(buf); 
    fclose(f); 
    ESP_LOGI(TAG, "Screenshot saved: %s", fname);
    return ESP_OK;
}