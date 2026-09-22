// ES8311 sequence adapted from components/audio_player/es8311.c (LinIT-L).
// Unlike the full app, missing codec / failed register access must be reported.
#include <math.h>
#include <stdint.h>
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "driver/i2s_std.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "board_pins.h"
#include "test_audio.h"

static const char *TAG = "TEST_AUDIO";
static i2s_chan_handle_t tx;
static uint8_t codec_addr;
static bool ready;

static esp_err_t reg_write(uint8_t reg, uint8_t value)
{
    uint8_t bytes[] = {reg, value};
    return i2c_master_write_to_device(I2C_NUM_0, codec_addr, bytes, 2, pdMS_TO_TICKS(100));
}

static esp_err_t reg_read(uint8_t reg, uint8_t *value)
{
    return i2c_master_write_read_device(I2C_NUM_0, codec_addr, &reg, 1, value, 1,
                                      pdMS_TO_TICKS(100));
}

esp_err_t test_audio_init(void)
{
    gpio_config_t pa = {.pin_bit_mask = 1ULL << PIN_PA_ENABLE, .mode = GPIO_MODE_OUTPUT};
    esp_err_t err = gpio_config(&pa);
    if (err != ESP_OK) return err;
    gpio_set_level(PIN_PA_ENABLE, 0);
    i2c_config_t bus = {
        .mode = I2C_MODE_MASTER, .sda_io_num = PIN_I2C_SDA, .scl_io_num = PIN_I2C_SCL,
        .sda_pullup_en = true, .scl_pullup_en = true, .master.clk_speed = 100000,
    };
    err = i2c_param_config(I2C_NUM_0, &bus);
    if (err != ESP_OK) return err;
    err = i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0);
    if (err != ESP_OK) return err;
    for (int addr = 8; addr < 0x78; addr++) {
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        if (!cmd) return ESP_ERR_NO_MEM;
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, addr << 1, true);
        i2c_master_stop(cmd);
        err = i2c_master_cmd_begin(I2C_NUM_0, cmd, pdMS_TO_TICKS(30));
        i2c_cmd_link_delete(cmd);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "I2C ACK at 0x%02x", addr);
            if (addr == 0x18 || (addr == 0x1a && !codec_addr)) codec_addr = addr;
        }
    }
    if (!codec_addr) {
        ESP_LOGE(TAG, "ES8311 absent. Check power, SDA13/SCL14, pullups and soldering.");
        return ESP_ERR_NOT_FOUND;
    }

    i2s_chan_config_t channel = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    channel.auto_clear = true;
    channel.dma_desc_num = 4;
    channel.dma_frame_num = 256;
    err = i2s_new_channel(&channel, &tx, NULL);
    if (err != ESP_OK) return err;
    i2s_std_config_t cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(16000),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {.mclk = PIN_I2S_MCLK, .bclk = PIN_I2S_BCLK, .ws = PIN_I2S_WS,
                     .dout = PIN_I2S_DOUT, .din = I2S_GPIO_UNUSED},
    };
    err = i2s_channel_init_std_mode(tx, &cfg);
    if (err != ESP_OK) goto cleanup;
    err = i2s_channel_enable(tx);
    if (err != ESP_OK) goto cleanup;

    const uint8_t regs[][2] = {
        {0x44,0x08},{0x44,0x08},{0x01,0x30},{0x02,0x00},{0x03,0x10},
        {0x16,0x24},{0x04,0x10},{0x05,0x00},{0x0b,0x00},{0x0c,0x00},
        {0x10,0x1f},{0x11,0x7f},{0x00,0x80},{0x01,0x3f},{0x13,0x10},
        {0x1b,0x0a},{0x1c,0x6a},{0x44,0x08},
        {0x00,0x80},{0x01,0x3f},{0x09,0x0c},{0x0a,0x0c},{0x17,0xbf},
        {0x0e,0x02},{0x12,0x00},{0x14,0x1a},{0x0d,0x01},{0x15,0x40},
        {0x37,0x08},{0x45,0x00},
        {0x02,0x00},{0x03,0x10},{0x04,0x10},{0x05,0x00},{0x06,0x03},
        {0x07,0x00},{0x08,0xff},{0x32,0xa8}, // -12 dB digital volume
    };
    for (size_t i = 0; i < sizeof(regs) / sizeof(regs[0]); i++) {
        err = reg_write(regs[i][0], regs[i][1]);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Register 0x%02x write failed: %s", regs[i][0], esp_err_to_name(err));
            goto disable;
        }
    }
    uint8_t volume = 0;
    err = reg_read(0x32, &volume);
    if (err != ESP_OK) goto disable;
    if (volume != 0xa8) { err = ESP_ERR_INVALID_RESPONSE; goto disable; }
    ready = true;
    ESP_LOGI(TAG, "Codec 0x%02x register readback OK; I2S TX 16kHz. Speaker NOT yet verified.", codec_addr);
    return ESP_OK;
disable:
    gpio_set_level(PIN_PA_ENABLE, 0);
    i2s_channel_disable(tx);
cleanup:
    i2s_del_channel(tx);
    tx = NULL;
    return err;
}

esp_err_t test_audio_play(void)
{
    if (!ready) return ESP_ERR_INVALID_STATE;
    ESP_LOGI(TAG, "Playing short 880Hz tone; confirm sound by ear.");
    int16_t samples[256 * 2];
    esp_err_t err = ESP_OK;
    gpio_set_level(PIN_PA_ENABLE, 1);
    vTaskDelay(pdMS_TO_TICKS(20));
    // 0.5 s, 12.5% full-scale, -12 dB codec gain; 20 ms click-suppression ramps.
    for (int offset = 0; offset < 8000; offset += 256) {
        int count = (8000 - offset < 256) ? 8000 - offset : 256;
        for (int j = 0; j < count; j++) {
            int t = offset + j;
            float gain = t < 320 ? t / 320.0f : (t > 7680 ? (8000 - t) / 320.0f : 1.0f);
            int16_t v = (int16_t)(4096 * gain * sinf(2.0f * 3.14159265f * 880 * t / 16000));
            samples[j * 2] = samples[j * 2 + 1] = v;
        }
        size_t written = 0, bytes = count * 2 * sizeof(int16_t);
        err = i2s_channel_write(tx, samples, bytes, &written, 1000);
        if (err != ESP_OK || written != bytes) {
            if (err == ESP_OK) err = ESP_FAIL;
            break;
        }
    }
    vTaskDelay(pdMS_TO_TICKS(100)); // Let the queued tail play before disabling the amplifier.
    gpio_set_level(PIN_PA_ENABLE, 0);
    ESP_LOGI(TAG, "Tone transfer: %s (not an acoustic measurement)", esp_err_to_name(err));
    return err;
}
