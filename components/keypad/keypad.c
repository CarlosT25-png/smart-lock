#include <stdio.h>
#include "keypad.h"

static const char *TAG = "KEYPAD";

struct keypad_config_t
{
  uint8_t rows[4];
  uint8_t cols[4];
};

esp_err_t init_keypad(keypad_handle_t *out_handle, uint8_t r1, uint8_t r2, uint8_t r3, uint8_t r4, uint8_t c1, uint8_t c2, uint8_t c3, uint8_t c4)
{
  keypad_handle_t handle = malloc(sizeof(struct keypad_config_t));
  if (handle == NULL)
  {
    return ESP_ERR_NO_MEM;
  }

  handle->rows[0] = r1;
  handle->rows[1] = r2;
  handle->rows[2] = r3;
  handle->rows[3] = r4;
  handle->cols[0] = c1;
  handle->cols[1] = c2;
  handle->cols[2] = c3;
  handle->cols[3] = c4;

  for (int i = 0; i < 4; i++)
  {
    gpio_reset_pin(handle->rows[i]);
    gpio_set_pull_mode(handle->rows[i], GPIO_PULLUP_ONLY);
    gpio_set_direction(handle->rows[i], GPIO_MODE_INPUT);

    gpio_reset_pin(handle->cols[i]);
    gpio_set_pull_mode(handle->cols[i], GPIO_PULLUP_ONLY);
    gpio_set_direction(handle->cols[i], GPIO_MODE_INPUT);
  }

  *out_handle = handle;

  ESP_LOGI(TAG, "keypad initialized");
  return ESP_OK;
}

esp_err_t keypad_get_value(keypad_handle_t handle, char *value)
{
  int r_found = -1;
  int c_found = -1;

  char keys[4][4] = {
      {'1', '2', '3', 'A'},
      {'4', '5', '6', 'B'},
      {'7', '8', '9', 'C'},
      {'*', '0', '#', 'D'}};

  for (int i = 0; i < 4; i++)
  {
    gpio_set_direction(handle->rows[i], GPIO_MODE_OUTPUT);
    gpio_set_level(handle->rows[i], 0);

    for (int j = 0; j < 4; j++)
    {
      if (gpio_get_level(handle->cols[j]) == 0)
      {
        r_found = i;
        c_found = j;
      }
    }

    gpio_set_direction(handle->rows[i], GPIO_MODE_INPUT);
  }

  if (r_found != -1 && c_found != -1)
  {
    vTaskDelay(pdMS_TO_TICKS(50)); // debounce
    *value = keys[r_found][c_found]; 
    return ESP_OK;
  }

  return ESP_ERR_NOT_FOUND;
}