#include <stdio.h>
#include "solenoid-lock-12v.h"

static const char *TAG = "solenoid";

struct solenoid_config_t
{
  uint8_t gpio_port;
};

esp_err_t solenoid_init(solenoid_handle_t *out_handler, __uint8_t gpio_port)
{
  solenoid_handle_t handle = malloc(sizeof(struct solenoid_config_t));

  if (handle == NULL) {
    return ESP_ERR_NO_MEM;
  }

  handle->gpio_port = gpio_port;

  gpio_reset_pin(gpio_port);
  gpio_set_direction(gpio_port, GPIO_MODE_OUTPUT);
  gpio_set_level(gpio_port, false); // lock by default
  *out_handler = handle;

  ESP_LOGI(TAG, "initialized");

  return ESP_OK;
}

esp_err_t solenoid_unlock(solenoid_handle_t handle)
{
  gpio_set_level(handle->gpio_port, true);
  return ESP_OK;
}

esp_err_t solenoid_lock(solenoid_handle_t handle)
{
  gpio_set_level(handle->gpio_port, false);
  return ESP_OK;
}
