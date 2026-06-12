#ifndef DS3231_H
#define DS3231_H

#include <stdint.h>
#include "esp_err.h"
#include "driver/i2c_master.h"

#define DS3231_I2C_ADDR 0x68
#define DS3231_INT_PIN 4

esp_err_t ds3231_init(i2c_master_bus_handle_t bus);
esp_err_t ds3231_arm_alarm_10_seconds(void);
esp_err_t ds3231_arm_alarm_1_minute(void);
esp_err_t ds3231_clear_alarm(void);

#endif