#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"

struct keypad_config_t;

typedef struct keypad_config_t *keypad_handle_t;

esp_err_t init_keypad(keypad_handle_t *out_handle, uint8_t r1, uint8_t r2, uint8_t r3, uint8_t r4, uint8_t c1, uint8_t c2, uint8_t c3, uint8_t c4);

esp_err_t keypad_get_value(keypad_handle_t handle, char *value);