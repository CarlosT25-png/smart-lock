#include <stdio.h>
#include "oled-ssd1306.h"

ssd1306_handle_t disp;

static char *TAG = "OLED";

esp_err_t oled_init()
{
    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = 21,
        .scl_io_num = 22,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    i2c_master_bus_handle_t bus_handle;
    i2c_new_master_bus(&bus_cfg, &bus_handle);

    ssd1306_config_t cfg = {
        .bus = SSD1306_I2C,
        .width = 128,
        .height = 64,
        .iface.i2c = {
            .port = I2C_NUM_0,
            .addr = 0x3C,
            .rst_gpio = 17,
        },
    };

    ssd1306_new_i2c(&cfg, &disp);
    ssd1306_clear(disp);
    ESP_LOGI(TAG, "initialized");

    return ESP_OK;
}

esp_err_t oled_print_string(char *text, const int x, const int y)
{
    ssd1306_draw_text(disp, x, y, text, true);
    ssd1306_display(disp);

    return ESP_OK;
}

esp_err_t oled_clear_screen()
{
    ssd1306_clear(disp);

    return ESP_OK;
}