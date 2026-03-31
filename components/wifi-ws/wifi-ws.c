/* WiFi station Example
   This example code is in the Public Domain (or CC0 licensed, at your option.)
*/
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_websocket_client.h"
#include "esp_crt_bundle.h"
#include "cJSON.h"

#include "lwip/err.h"
#include "lwip/sys.h"

// --- Configuration ---
#define EXAMPLE_ESP_WIFI_SSID "YOUR_WIFI_SSID"
#define EXAMPLE_ESP_WIFI_PASS "YOUR_WIFI_PWD"
#define EXAMPLE_ESP_MAXIMUM_RETRY 5

#define ESP_WIFI_SAE_MODE WPA3_SAE_PWE_BOTH
#define EXAMPLE_H2E_IDENTIFIER ""
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_PSK

static EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1

static const char *TAG = "wifi station";
static int s_retry_num = 0;


// If you are using secure web socket, you need this connect to your server
// generate this using the following command
// openssl s_client -showcerts -servername {URL} -connect smart-l.onrender.com:443 </dev/null
// get the last certificate from the command output
static const char *server_root_cert =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIDejCCAmKgAwIBAgIQf+UwvzMTQ77dghYQST2KGzANBgkqhkiG9w0BAQsFADBX\n"
    "MQswCQYDVQQGEwJCRTEZMBcGA1UEChMQR2xvYmFsU2lnbiBudi1zYTEQMA4GA1UE\n"
    "CxMHUm9vdCBDQTEbMBkGA1UEAxMSR2xvYmFsU2lnbiBSb290IENBMB4XDTIzMTEx\n"
    "NTAzNDMyMVoXDTI4MDEyODAwMDA0MlowRzELMAkGA1UEBhMCVVMxIjAgBgNVBAoT\n"
    "GUdvb2dsZSBUcnVzdCBTZXJ2aWNlcyBMTEMxFDASBgNVBAMTC0dUUyBSb290IFI0\n"
    "MHYwEAYHKoZIzj0CAQYFK4EEACIDYgAE83Rzp2iLYK5DuDXFgTB7S0md+8Fhzube\n"
    "Rr1r1WEYNa5A3XP3iZEwWus87oV8okB2O6nGuEfYKueSkWpz6bFyOZ8pn6KY019e\n"
    "WIZlD6GEZQbR3IvJx3PIjGov5cSr0R2Ko4H/MIH8MA4GA1UdDwEB/wQEAwIBhjAd\n"
    "BgNVHSUEFjAUBggrBgEFBQcDAQYIKwYBBQUHAwIwDwYDVR0TAQH/BAUwAwEB/zAd\n"
    "BgNVHQ4EFgQUgEzW63T/STaj1dj8tT7FavCUHYwwHwYDVR0jBBgwFoAUYHtmGkUN\n"
    "l8qJUC99BM00qP/8/UswNgYIKwYBBQUHAQEEKjAoMCYGCCsGAQUFBzAChhpodHRw\n"
    "Oi8vaS5wa2kuZ29vZy9nc3IxLmNydDAtBgNVHR8EJjAkMCKgIKAehhxodHRwOi8v\n"
    "Yy5wa2kuZ29vZy9yL2dzcjEuY3JsMBMGA1UdIAQMMAowCAYGZ4EMAQIBMA0GCSqG\n"
    "SIb3DQEBCwUAA4IBAQAYQrsPBtYDh5bjP2OBDwmkoWhIDDkic574y04tfzHpn+cJ\n"
    "odI2D4SseesQ6bDrarZ7C30ddLibZatoKiws3UL9xnELz4ct92vID24FfVbiI1hY\n"
    "+SW6FoVHkNeWIP0GCbaM4C6uVdF5dTUsMVs/ZbzNnIdCp5Gxmx5ejvEau8otR/Cs\n"
    "kGN+hr/W5GvT1tMBjgWKZ1i4//emhA1JG1BbPzoLJQvyEotc03lXjTaCzv8mEbep\n"
    "8RqZ7a2CPsgRbuvTPBwcOMBBmuFeU88+FSBX6+7iP0il8b4Z0QFqIwwMHfs/L6K1\n"
    "vepuoxtGzi4CZ68zJpiq1UvSqTbFJjtbD4seiMHl\n"
    "-----END CERTIFICATE-----\n";

// --- Hardware Callback Function Pointers ---
// Define a type for a function that takes no arguments and returns nothing
typedef void (*lock_action_cb_t)(void);

// Static variables to hold the specific functions passed in from main.c
static lock_action_cb_t s_on_lock_cb = NULL;
static lock_action_cb_t s_on_unlock_cb = NULL;

// --- Wi-Fi Event Handler ---
static void event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
  {
    esp_wifi_connect();
  }
  else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
  {
    if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY)
    {
      esp_wifi_connect();
      s_retry_num++;
      ESP_LOGI(TAG, "retry to connect to the AP");
    }
    else
    {
      xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
    }
    ESP_LOGI(TAG, "connect to the AP fail");
  }
  else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
  {
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
    s_retry_num = 0;
    xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
  }
}

void wifi_init_sta(void)
{
  s_wifi_event_group = xEventGroupCreate();
  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  esp_netif_create_default_wifi_sta();

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  esp_event_handler_instance_t instance_any_id;
  esp_event_handler_instance_t instance_got_ip;
  ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, &instance_any_id));
  ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL, &instance_got_ip));

  wifi_config_t wifi_config = {
      .sta = {
          .ssid = EXAMPLE_ESP_WIFI_SSID,
          .password = EXAMPLE_ESP_WIFI_PASS,
          .threshold.authmode = ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD,
          .sae_pwe_h2e = ESP_WIFI_SAE_MODE,
          .sae_h2e_identifier = EXAMPLE_H2E_IDENTIFIER,
      },
  };
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
  ESP_ERROR_CHECK(esp_wifi_start());

  ESP_LOGI(TAG, "wifi_init_sta finished.");

  EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT, pdFALSE, pdFALSE, portMAX_DELAY);

  if (bits & WIFI_CONNECTED_BIT)
  {
    ESP_LOGI(TAG, "connected to ap SSID:%s", EXAMPLE_ESP_WIFI_SSID);
  }
  else if (bits & WIFI_FAIL_BIT)
  {
    ESP_LOGI(TAG, "Failed to connect to SSID:%s", EXAMPLE_ESP_WIFI_SSID);
  }
  else
  {
    ESP_LOGE(TAG, "UNEXPECTED EVENT");
  }
}

// Custom code - WebSocket
static void send_status_update(esp_websocket_client_handle_t client, bool is_locked)
{
  char payload[64];
  snprintf(payload, sizeof(payload), "{\"type\":\"statusUpdate\",\"locked\":%s}", is_locked ? "true" : "false");

  esp_websocket_client_send_text(client, payload, strlen(payload), portMAX_DELAY);
  ESP_LOGI(TAG, "Sent status update: %s", payload);
}

static void websocket_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
  esp_websocket_event_data_t *data = (esp_websocket_event_data_t *)event_data;

  switch (event_id)
  {
  case WEBSOCKET_EVENT_CONNECTED:
    ESP_LOGI(TAG, "WEBSOCKET_EVENT_CONNECTED");
    break;
  case WEBSOCKET_EVENT_DISCONNECTED:
    ESP_LOGI(TAG, "WEBSOCKET_EVENT_DISCONNECTED");
    break;
  case WEBSOCKET_EVENT_DATA:
    ESP_LOGI(TAG, "WEBSOCKET_EVENT_DATA");

    if (data->op_code == 0x1)
    {
      char *json_str = malloc(data->data_len + 1);
      if (json_str == NULL)
      {
        ESP_LOGE(TAG, "Failed to allocate memory for JSON string");
        break;
      }
      memcpy(json_str, data->data_ptr, data->data_len);
      json_str[data->data_len] = '\0';

      cJSON *root = cJSON_Parse(json_str);
      if (root != NULL)
      {
        cJSON *type = cJSON_GetObjectItem(root, "type");

        if (type != NULL && cJSON_IsString(type))
        {
          esp_websocket_client_handle_t client = (esp_websocket_client_handle_t)handler_args;

          if (strcmp(type->valuestring, "lock") == 0)
          {
            ESP_LOGI(TAG, "Action: LOCKING the door");

            send_status_update(client, true);

            if (s_on_lock_cb != NULL)
              s_on_lock_cb();
          }
          else if (strcmp(type->valuestring, "unlock") == 0)
          {
            ESP_LOGI(TAG, "Action: UNLOCKING the door");

            send_status_update(client, false);

            if (s_on_unlock_cb != NULL)
              s_on_unlock_cb();
          }
        }
        cJSON_Delete(root);
      }
      else
      {
        ESP_LOGE(TAG, "Failed to parse JSON");
      }
      free(json_str);
    }
    break;
  case WEBSOCKET_EVENT_ERROR:
    ESP_LOGI(TAG, "WEBSOCKET_EVENT_ERROR");
    break;
  }
}

void websocket_app_start(void (*lock_func)(void), void (*unlock_func)(void))
{
  ESP_LOGI(TAG, "Connecting to websocket server...");

  s_on_lock_cb = lock_func;
  s_on_unlock_cb = unlock_func;

  const esp_websocket_client_config_t ws_cfg = {
      .uri = "wss://example.org", // REPLACE WITH YOUR OWN WEBSOCKET
      .cert_pem = server_root_cert,
  };

  esp_websocket_client_handle_t client = esp_websocket_client_init(&ws_cfg);
  esp_websocket_register_events(client, WEBSOCKET_EVENT_ANY, websocket_event_handler, (void *)client);
  esp_websocket_client_start(client);
}

void init_wifi(void)
{
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
  {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);

  if (CONFIG_LOG_MAXIMUM_LEVEL > CONFIG_LOG_DEFAULT_LEVEL)
  {
    esp_log_level_set("wifi", CONFIG_LOG_MAXIMUM_LEVEL);
  }

  ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");
  wifi_init_sta();
}