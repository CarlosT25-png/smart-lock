#pragma once

#include <driver/i2c_master.h>
#include <esp_err.h>
#include <esp_log.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "ssd1306.h"

esp_err_t oled_init();

esp_err_t oled_print_string(char *text, const int x, const int y);

esp_err_t oled_clear_screen();