#ifndef SHT30_H
#define SHT30_H

#include <stdint.h>
#include "esp_err.h"
#include "driver/i2c_master.h"


#define I2C_SDA_PIN 8
#define I2C_SCL_PIN 9



typedef struct {
    float temperatura;
    float umidade;
    i2c_master_dev_handle_t dev_handle;
} SensorSHT30;


esp_err_t sht30_init(i2c_master_bus_handle_t bus_handle, uint8_t endereco);
esp_err_t sht30_read(void);


/* --- Getters --- */
float sht30_get_temperatura(void);
float sht30_get_umidade(void);

#endif
