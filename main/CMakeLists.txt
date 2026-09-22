idf_component_register(
    SRCS "main.c" "test_audio.c" "test_touch.c"
    INCLUDE_DIRS "."
    REQUIRES st7305 board_battery touch_panel driver esp_adc esp_timer esp_psram
             esp_driver_i2s esp_driver_i2c sdmmc spi_flash)
