idf_component_register(SRCS "wifi-ws.c"
                    INCLUDE_DIRS "include"
                    REQUIRES esp_wifi nvs_flash bt freertos esp_websocket_client)
