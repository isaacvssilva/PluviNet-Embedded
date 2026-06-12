idf_component_register(
    SRCS
        "src/main.c"              
        "src/sensor_hall.c"
        "src/sht30.c"
        "src/wifi.c"
        "src/mqtt.c"
        "src/ds3231.c"        
    INCLUDE_DIRS
        "."
    REQUIRES
        nvs_flash wifi_provisioning esp_wifi esp_event driver esp_phy mqtt
)
