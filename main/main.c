#include <stdio.h>
#include <string.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "keypad.h"
#include "oled-ssd1306.h"
#include "rfid-rc522.h"
#include "solenoid-lock-12v.h"
#include "wifi-ws.h"
#include "esp_netif_sntp.h"

// KEYPAD PORTS
#define R1 13
#define R2 12
#define R3 14
#define R4 27
#define C1 26
#define C2 25
#define C3 33
#define C4 32

// SOLENOID GPIO
#define SOLENOID_GPIO 19

// variables
char curr_pwd[5] = "";
char hash_pwd[5] = "";
int input_count = 0;
volatile bool is_busy = false; // this is helper variable, to ensure only one function is printing on the OLED screen at the time

// handles
solenoid_handle_t solenoid_handle = NULL;

// helper functions
static void obtain_time(void)
{
    ESP_LOGI("SNTP", "Initializing SNTP to get the correct time...");

    // configure NTP to pull the time from pool.ntp.org
    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
    esp_netif_sntp_init(&config);

    if (esp_netif_sntp_sync_wait(pdMS_TO_TICKS(15000)) != ESP_OK)
    {
        ESP_LOGE("SNTP", "Failed to update system time!");
    }
    else
    {
        time_t now = 0;
        struct tm timeinfo = {0};
        time(&now);
        localtime_r(&now, &timeinfo);
        ESP_LOGI("SNTP", "Time synced successfully! Current time: %s", asctime(&timeinfo));
    }
}

void reset_state(void)
{
    // Reset for next attempt
    solenoid_lock(solenoid_handle);
    oled_clear_screen();
    memset(curr_pwd, 0, sizeof(curr_pwd));
    memset(hash_pwd, 0, sizeof(hash_pwd));
    input_count = 0;
}

void access_granted(void)
{
    is_busy = true;
    solenoid_unlock(solenoid_handle);
    oled_clear_screen();
    oled_print_string("ACCESS GRANTED!", 4, 28);
    vTaskDelay(pdMS_TO_TICKS(3000));
    reset_state();
    is_busy = false;
}

void access_denied(void)
{
    is_busy = true; // Tell the main loop to stop drawing
    oled_clear_screen();
    oled_print_string("WRONG PWD!", 4, 28);
    vTaskDelay(pdMS_TO_TICKS(1000));
    reset_state();
    is_busy = false; // Tell the main loop it can draw again
}

void app_main(void)
{
    keypad_handle_t handle = NULL;
    init_keypad(&handle, R1, R2, R3, R4, C1, C2, C3, C4);

    spi_device_handle_t spi;

    oled_init();
    rfid_init(&spi);
    solenoid_init(&solenoid_handle, SOLENOID_GPIO); // the solenoid is lock when it's initialized
    init_wifi();
    obtain_time(); // this is needed to generate a valid certificate to connect my secure web socket
    websocket_app_start(reset_state, access_granted);

    char key_pressed = '\0';
    const char *pwd = "1111"; // HARDCODED PASSWORD - REPLACE

    while (1)
    {

        // welcome text
        if (!is_busy)
        {
            oled_print_string("Smart Lock", 35, 0);
            oled_print_string("Swipe your badge", 0, 16);
            oled_print_string("or type your password", 0, 28);
            oled_print_string(hash_pwd, 4, 40);
        }

        // check rfid
        rfid_read_access(spi, access_granted, access_denied);

        // check keypad

        if (keypad_get_value(handle, &key_pressed) == ESP_OK)
        {
            if (key_pressed != '\0' && input_count < 4)
            {
                printf("Keypad pressed: %c\n", key_pressed);

                // Append the key correctly
                curr_pwd[input_count] = key_pressed;
                hash_pwd[input_count] = '*';
                input_count++;

                // Keep the strings valid
                curr_pwd[input_count] = '\0';
                hash_pwd[input_count] = '\0';

                key_pressed = '\0'; // Reset for next scan
            }
        }

        oled_print_string(hash_pwd, 4, 40);

        if (input_count == 4)
        {
            vTaskDelay(pdMS_TO_TICKS(500)); // Brief pause to see the 4th star

            if (strcmp(curr_pwd, pwd) == 0)
            {
                access_granted();
            }
            else
            {
                access_denied();
            }
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}