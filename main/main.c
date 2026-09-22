#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/sdmmc_host.h"
#include "sdmmc_cmd.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_psram.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "st7305.h"
#include "board_battery.h"
#include "board_pins.h"
#include "test_audio.h"
#include "test_touch.h"

static const char *TAG = "HW_TEST";
static st7305_handle_t lcd;
static bool lcd_ready;
static esp_err_t audio_status = ESP_ERR_INVALID_STATE;
static esp_err_t battery_status = ESP_ERR_INVALID_STATE;
static board_battery_status_t battery;
static const char *ram_status = "NOT TESTED";
static const char *sd_status = "PRESS S TO TEST";
static int page;
static unsigned key_count[3];
static const int key_pins[] = {PIN_BOOT, PIN_KEY, PIN_PWR};
static const char *key_names[] = {"BOOT", "KEY", "PWR"};

static void memory_test(void)
{
    size_t size = esp_psram_is_initialized() ? esp_psram_get_size() : 0;
    ESP_LOGI(TAG, "PSRAM detected: %u bytes", (unsigned)size);
    if (!size) { ram_status = "NOT DETECTED"; return; }
    const size_t n = 64 * 1024;
    volatile uint32_t *buf = heap_caps_malloc(n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!buf) { ram_status = "ALLOC FAILED"; return; }
    bool ok = true;
    for (int pass = 0; pass < 2 && ok; pass++) {
        uint32_t mask = pass ? 0x55555555 : 0xaaaaaaaa;
        for (size_t i = 0; i < n / 4; i++) buf[i] = mask ^ (uint32_t)i;
        for (size_t i = 0; i < n / 4; i++) {
            if (buf[i] != (mask ^ (uint32_t)i)) { ok = false; break; }
        }
    }
    free((void *)buf);
    ram_status = ok ? "64KB R/W OK" : "R/W FAILED";
    ESP_LOGI(TAG, "PSRAM sample test: %s (not a full stress test)", ram_status);
}

static void read_battery(void)
{
    battery_status = board_battery_read(&battery);
    if (battery_status == ESP_OK) {
        ESP_LOGI(TAG, "BAT estimate=%" PRIu32 " mV (GPIO4 x3). Compare with multimeter; not charger/SOC test.", battery.voltage_mv);
    } else ESP_LOGE(TAG, "BAT ADC: %s", esp_err_to_name(battery_status));
}

static void draw_page(void)
{
    if (!lcd_ready) return;
    st7305_clear(&lcd, page == 2 ? ST7305_COLOR_BLACK : ST7305_COLOR_WHITE);
    char line[64];
    if (page == 0) {
        st7305_draw_text(&lcd, 8, 8, "RLCD HARDWARE TEST v2");
        snprintf(line, sizeof(line), "UPTIME: %lld SEC", esp_timer_get_time() / 1000000);
        st7305_draw_text(&lcd, 8, 32, line);
        st7305_draw_text(&lcd, 8, 56, "LCD: CHECK IMAGE BY EYE");
        snprintf(line, sizeof(line), "PSRAM: %s", ram_status);
        st7305_draw_text(&lcd, 8, 80, line);
        st7305_draw_text(&lcd, 8, 104, audio_status == ESP_OK ? "AUDIO: I2C/I2S READY" : "AUDIO: INIT FAILED - SEE USB");
        if (battery_status == ESP_OK) snprintf(line, sizeof(line), "BAT: %" PRIu32 " mV (CHECK METER)", battery.voltage_mv);
        else snprintf(line, sizeof(line), "BAT: ADC ERROR");
        st7305_draw_text(&lcd, 8, 128, line);
        snprintf(line, sizeof(line), "SD: %s", sd_status);
        st7305_draw_text(&lcd, 8, 152, line);
        snprintf(line, sizeof(line), "KEYS B:%u K:%u P:%u", key_count[0], key_count[1], key_count[2]);
        st7305_draw_text(&lcd, 8, 176, line);
        st7305_draw_text(&lcd, 8, 204, "BOOT: PATTERN  KEY: BEEP");
        st7305_draw_text(&lcd, 8, 228, "PWR: COUNT ONLY / NO SLEEP");
        st7305_draw_text(&lcd, 8, 252, "USB: L=LCD A=AUDIO S=SD");
        snprintf(line, sizeof(line), "TOUCH:%s USB T=TEST", test_touch_name());
        st7305_draw_text(&lcd, 8, 276, line);
    } else if (page == 5) {
        test_touch_draw(&lcd);
    } else if (page >= 3) {
        for (int y = 0; y < ST7305_HEIGHT; y++) {
            for (int x = 0; x < ST7305_WIDTH; x++) {
                bool black = page == 3 ? ((x / 20 + y / 20) & 1) : ((x & 1) != 0);
                st7305_draw_pixel(&lcd, x, y, black ? ST7305_COLOR_BLACK : ST7305_COLOR_WHITE);
            }
        }
    }
    // Border is deliberately omitted on uniform white/black test pages.
    if (page == 0) {
        for (int x = 0; x < 400; x++) {
            st7305_draw_pixel(&lcd, x, 0, 0); st7305_draw_pixel(&lcd, x, 299, 0);
        }
        for (int y = 0; y < 300; y++) {
            st7305_draw_pixel(&lcd, 0, y, 0); st7305_draw_pixel(&lcd, 399, y, 0);
        }
    }
    esp_err_t err = st7305_flush(&lcd);
    if (err != ESP_OK) ESP_LOGE(TAG, "LCD SPI flush: %s", esp_err_to_name(err));
}

static void sd_test(void)
{
    // Raw read only: no filesystem mount, format, erase or writes.
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.flags = SDMMC_HOST_FLAG_1BIT;
    host.max_freq_khz = 5000;
    sdmmc_slot_config_t slot = SDMMC_SLOT_CONFIG_DEFAULT();
    slot.width = 1;
    slot.clk = PIN_SD_CLK; slot.cmd = PIN_SD_CMD; slot.d0 = PIN_SD_D0;
    slot.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;
    sdmmc_card_t card = {0};
    esp_err_t err = sdmmc_host_init();
    if (err != ESP_OK) { sd_status = "HOST INIT FAILED"; return; }
    err = sdmmc_host_init_slot(host.slot, &slot);
    if (err == ESP_OK) err = sdmmc_card_init(&host, &card);
    if (err == ESP_OK) {
        void *sector = heap_caps_malloc(card.csd.sector_size, MALLOC_CAP_DMA);
        if (!sector) err = ESP_ERR_NO_MEM;
        else { err = sdmmc_read_sectors(&card, sector, 0, 1); free(sector); }
        ESP_LOGI(TAG, "SD capacity: %llu MiB", (unsigned long long)card.csd.capacity * card.csd.sector_size / (1024 * 1024));
    }
    sd_status = err == ESP_OK ? "SECTOR READ OK" : "ABSENT / TEST FAILED";
    ESP_LOGI(TAG, "SD read-only test: %s (%s)", sd_status, esp_err_to_name(err));
    sdmmc_host_deinit();
}

static void command(char c)
{
    if (c == 'a' || c == 'A') {
        esp_err_t err = test_audio_play();
        if (err != ESP_OK) ESP_LOGE(TAG, "Audio test unavailable/failed: %s", esp_err_to_name(err));
    } else if (c == 'l' || c == 'L') {
        page = (page + 1) % 6;
        ESP_LOGI(TAG, "LCD page %d: 0=status 1=white 2=black 3=checker 4=vertical stripes 5=touch", page);
    } else if (c == 't' || c == 'T') {
        page = page == 5 ? 0 : 5;
        test_touch_report();
    } else if (c == 'c' || c == 'C') {
        test_touch_clear();
    } else if (c == 's' || c == 'S') sd_test();
    else if (c == 'b' || c == 'B') read_battery();
    else if (c == 'h' || c == 'H' || c == '?') {
        ESP_LOGI(TAG, "Commands: L next LCD pattern, A low-volume tone, B battery, S read-only SD, T touch/status, C clear touch, H help");
        test_touch_report();
    }
    else return;
    draw_page();
}

void app_main(void)
{
    vTaskDelay(pdMS_TO_TICKS(1500));
    ESP_LOGI(TAG, "=== RLCD MINIMAL HARDWARE TEST v2 (touch) ===");
    ESP_LOGI(TAG, "Reset reason=%d; no Wi-Fi/BLE/apps/sleep/USB disk", esp_reset_reason());
    uint32_t flash_size = 0;
    esp_err_t err = esp_flash_get_size(NULL, &flash_size);
    ESP_LOGI(TAG, "Flash: %s, %" PRIu32 " bytes", esp_err_to_name(err), flash_size);
    memory_test();
    gpio_config_t keys = {.pin_bit_mask = (1ULL << PIN_BOOT) | (1ULL << PIN_KEY) | (1ULL << PIN_PWR),
                          .mode = GPIO_MODE_INPUT, .pull_up_en = GPIO_PULLUP_ENABLE};
    err = gpio_config(&keys);
    ESP_LOGI(TAG, "Button GPIO init: %s", esp_err_to_name(err));
    read_battery();
    st7305_config_t cfg = st7305_default_config();
    cfg.dc_gpio = PIN_LCD_DC; cfg.rst_gpio = PIN_LCD_RST;
    cfg.spi_cs = PIN_LCD_CS; cfg.spi_sclk = PIN_LCD_SCK; cfg.spi_mosi = PIN_LCD_MOSI;
    cfg.spi_freq = 4; // Conservative 4 MHz for first board bring-up.
    err = st7305_init(&lcd, &cfg);
    lcd_ready = err == ESP_OK;
    ESP_LOGI(TAG, "LCD driver init: %s. No panel readback; inspect screen visually.", esp_err_to_name(err));
    draw_page();
    audio_status = test_audio_init();
    ESP_LOGI(TAG, "Audio init: %s", esp_err_to_name(audio_status));
    draw_page();
    if (audio_status == ESP_OK) test_audio_play();
    test_touch_init();
    draw_page();
    int flags = fcntl(STDIN_FILENO, F_GETFL);
    if (flags >= 0) fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
    ESP_LOGI(TAG, "READY. BOOT=pattern KEY=tone PWR=count. USB: L/A/B/S/T/C/H. SD test is manual.");
    int stable[3] = {1,1,1}, last[3] = {1,1,1}, ticks[3] = {0};
    int64_t heartbeat = 0;
    int64_t touch_frame = 0;
    bool touch_dirty = false;
    while (true) {
        char bytes[32];
        int n = read(STDIN_FILENO, bytes, sizeof(bytes));
        for (int i = 0; i < n; i++) command(bytes[i]);
        for (int i = 0; i < 3; i++) {
            int level = gpio_get_level(key_pins[i]);
            if (level != last[i]) { last[i] = level; ticks[i] = 0; }
            else if (ticks[i] < 3) ticks[i]++;
            if (ticks[i] == 3 && stable[i] != level) {
                stable[i] = level;
                ESP_LOGI(TAG, "BUTTON %s GPIO%d %s", key_names[i], key_pins[i], level ? "UP" : "DOWN");
                if (!level) {
                    key_count[i]++;
                    if (i == 0) command('l');
                    if (i == 1) command('a');
                    draw_page();
                }
            }
        }
        int64_t now = esp_timer_get_time();
        touch_dirty |= test_touch_poll();
        if (page == 5 && touch_dirty && now - touch_frame >= 100000) {
            draw_page();
            touch_frame = now;
            touch_dirty = false;
        }
        if (now - heartbeat >= 2000000) {
            heartbeat = now;
            read_battery();
            ESP_LOGI(TAG, "ALIVE uptime=%lld s heap=%u keys=%d/%d/%d", now / 1000000,
                     (unsigned)esp_get_free_heap_size(), stable[0], stable[1], stable[2]);
            if (page == 0) draw_page();
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
