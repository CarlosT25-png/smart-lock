#pragma once

#include "esp_log.h"
#include "driver/gpio.h"

struct solenoid_config_t;

typedef struct solenoid_config_t *solenoid_handle_t;

esp_err_t solenoid_init(solenoid_handle_t *out_handler, __uint8_t gpio_port);

esp_err_t solenoid_unlock(solenoid_handle_t handle);

esp_err_t solenoid_lock(solenoid_handle_t handle);
