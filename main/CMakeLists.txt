idf_component_register(
    SRCS
        "src/main.c"              # renomeie o arquivo desejado para main.c
        "src/sensor_hall.c"
        "src/sht30.c"
        "src/wifi.c"
        "src/mqtt.c"
        "src/ds3231.c"        # necessário apenas para main_rtc_externo.c
    INCLUDE_DIRS
        "."
    REQUIRES
        nvs_flash wifi_provisioning esp_wifi esp_event driver esp_phy mqtt
)
