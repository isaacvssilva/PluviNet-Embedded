#include "inc/sht30.h"

#include <string.h>
#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_log.h"
#include "hal/i2c_types.h"

static const char *TAG = "SHT30";

static SensorSHT30 sht30;

esp_err_t sht30_init(i2c_master_bus_handle_t bus_handle, uint8_t endereco) {
    sht30.temperatura = 0.0f;
    sht30.umidade     = 0.0f;
 
    i2c_device_config_t dev_cfg = {
        .dev_addr_length        = I2C_ADDR_BIT_LEN_7,
        .device_address         = endereco,
        .scl_speed_hz           = 100000,
        .scl_wait_us            = 0,
        .flags.disable_ack_check = false,
    };
 
    esp_err_t ret = i2c_master_bus_add_device(bus_handle, &dev_cfg, &sht30.dev_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao adicionar dispositivo SHT30 no barramento I2C");
    }
    return ret;
}



esp_err_t sht30_read(void) {
    uint8_t cmd[2] = {0x2C, 0x06};
 
    esp_err_t ret = i2c_master_transmit(sht30.dev_handle, cmd, 2, -1);
    if (ret != ESP_OK) return ret;
 
    vTaskDelay(pdMS_TO_TICKS(20));
 
    uint8_t data[6] = {0};
    ret = i2c_master_receive(sht30.dev_handle, data, 6, -1);
    if (ret != ESP_OK) return ret;
 
    uint16_t raw_temp     = (data[0] << 8) | data[1];
    uint16_t raw_humidity = (data[3] << 8) | data[4];
 
    sht30.temperatura = -45.0f + (175.0f * raw_temp     / 65535.0f);
    sht30.umidade     = 100.0f *  raw_humidity           / 65535.0f;
 
    return ESP_OK;
}

/* ------------------------------------------------------------------ */
/*  Getters                                                            */
/* ------------------------------------------------------------------ */
float sht30_get_temperatura(void) {
    return sht30.temperatura;
}
 
float sht30_get_umidade(void) {
    return sht30.umidade;
}
