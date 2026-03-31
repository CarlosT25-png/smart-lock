# main/CMakeLists.txt
idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES keypad oled-ssd1306 driver rfid-rc522 solenoid-lock-12v wifi-ws
)

list(APPEND EXTRA_COMPONENTS_DIRS components)