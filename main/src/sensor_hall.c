
#include "inc/sensor_hall.h"
 
#include "driver/gpio.h"
#include "esp_attr.h"


/*
 * Variável em RTC slow memory.
 * - Sobrevive a todos os ciclos de deep sleep (quick wakeup ou full wakeup).
 * - Resetada apenas quando sensor_hall_set_pulse_count(0) é chamada.
 * - Acumulada incrementalmente no caminho de quick wakeup.
 */
static RTC_DATA_ATTR uint32_t pulse_count = 0;


void config_pin_hall(void)
{
    gpio_config_t io = {
        .pin_bit_mask = (1ULL << PIN_HALL),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    gpio_config(&io);
}


/* ------------------------------------------------------------------ */
/*  Getters e Setters                                                  */
/* ------------------------------------------------------------------ */


uint32_t sensor_hall_get_pulse_count(void) {
    return pulse_count;
}
 
void sensor_hall_set_pulse_count(uint32_t value) {
    pulse_count = value;
}
 
void sensor_hall_increment_pulse(void) {
    pulse_count++;
}
