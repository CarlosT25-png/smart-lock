idf_component_register(SRCS "rfid-rc522.c"
                    INCLUDE_DIRS "include"
                    REQUIRES driver esp_driver_spi esp_driver_gpio freertos esp_timer)
