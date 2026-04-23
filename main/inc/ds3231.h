#ifndef DS3231_H
#define DS3231_H

#include <stdint.h>
#include "esp_err.h"
#include "driver/i2c_master.h"

/* Endereço I2C fixo do DS3231 */
#define DS3231_I2C_ADDR  0x68


#define DS3231_INT_PIN   4

/**
 * @brief Adiciona o DS3231 ao barramento I2C e configura o pino INT
 *        como saída de alarme (modo INTCN=1).
 */
esp_err_t ds3231_init(i2c_master_bus_handle_t bus);

/**
 * @brief Programa o Alarm 1 para disparar daqui a @p minutes minutos.  
 * @param minutes Intervalo desejado (1–59 minutos).
 */
esp_err_t ds3231_arm_alarm(uint8_t minutes);

/**
 * @brief Limpa o flag A1F no registrador de status do DS3231.
 */
esp_err_t ds3231_clear_alarm(void);

#endif 