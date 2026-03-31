idf_component_register(
    SRCS "oled-ssd1306.c"
    INCLUDE_DIRS "include"
    REQUIRES driver esp_driver_i2c esp_driver_gpio freertos chill-sam__ssd1306
)