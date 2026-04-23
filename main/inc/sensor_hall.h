#ifndef SENSOR_HALL_H
#define SENSOR_HALL_H

#include <stdint.h>
#include "esp_attr.h"
/*
 * GPIO do sensor Hall.
 * No ESP32-C3, GPIOs 0–5 suportam wakeup por GPIO em deep sleep.
 */

#define PIN_HALL 5 
#define DEEP_SLEEP_TIME_US (10 * 1000 * 1000) // 10s

void config_pin_hall(void);


uint32_t sensor_hall_get_pulse_count(void);


void sensor_hall_set_pulse_count(uint32_t value);

void sensor_hall_increment_pulse(void);

#endif
