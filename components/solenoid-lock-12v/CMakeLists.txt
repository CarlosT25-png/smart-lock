idf_component_register(SRCS "solenoid-lock-12v.c"
                    INCLUDE_DIRS "include"
                    REQUIRES driver esp_driver_gpio freertos)