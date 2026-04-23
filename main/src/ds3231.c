#include "inc/ds3231.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "DS3231";


#define REG_SECONDS    0x00   /* BCD: segundos (0–59)            */
#define REG_MINUTES    0x01   /* BCD: minutos  (0–59)            */
#define REG_HOURS      0x02   /* BCD: horas    (0–23, modo 24h)  */
#define REG_ALM1_SEC   0x07   /* Alarm 1: segundos  + A1M1       */
#define REG_ALM1_MIN   0x08   /* Alarm 1: minutos   + A1M2       */
#define REG_ALM1_HOUR  0x09   /* Alarm 1: horas     + A1M3       */
#define REG_ALM1_DAY   0x0A   /* Alarm 1: dia/data  + A1M4       */
#define REG_CONTROL    0x0E   /* Controle: INTCN, A1IE           */
#define REG_STATUS     0x0F   /* Status:   A1F (flag de alarme)  */

/* Bit A1M (mask) em cada registrador de alarme */
#define A1Mx_BIT  (1 << 7)

static i2c_master_dev_handle_t s_dev = NULL;


static uint8_t bcd_to_dec(uint8_t bcd) { return (bcd >> 4) * 10 + (bcd & 0x0F); }
static uint8_t dec_to_bcd(uint8_t dec) { return ((dec / 10) << 4) | (dec % 10);  }


static esp_err_t ds3231_write_reg(uint8_t reg, uint8_t value)
{
    uint8_t buf[2] = {reg, value};
    return i2c_master_transmit(s_dev, buf, 2, pdMS_TO_TICKS(100));
}

static esp_err_t ds3231_read_reg(uint8_t reg, uint8_t *out)
{
    esp_err_t ret = i2c_master_transmit(s_dev, &reg, 1, pdMS_TO_TICKS(100));
    if (ret != ESP_OK) return ret;
    return i2c_master_receive(s_dev, out, 1, pdMS_TO_TICKS(100));
}


esp_err_t ds3231_init(i2c_master_bus_handle_t bus)
{
    i2c_device_config_t dev_cfg = {
        .dev_addr_length         = I2C_ADDR_BIT_LEN_7,
        .device_address          = DS3231_I2C_ADDR,
        .scl_speed_hz            = 100000,
        .scl_wait_us             = 0,
        .flags.disable_ack_check = false,
    };

    esp_err_t ret = i2c_master_bus_add_device(bus, &dev_cfg, &s_dev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao adicionar DS3231 no barramento I2C");
        return ret;
    }

    ret = ds3231_write_reg(REG_CONTROL, 0x05); /* 0b00000101 */
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao configurar registrador de controle");
    }
    return ret;
}

esp_err_t ds3231_arm_alarm(uint8_t minutes)
{
    if (s_dev == NULL) return ESP_ERR_INVALID_STATE;
    if (minutes == 0 || minutes > 59) return ESP_ERR_INVALID_ARG;

    uint8_t raw_min, raw_hour;
    esp_err_t ret;

    ret = ds3231_read_reg(REG_MINUTES, &raw_min);
    if (ret != ESP_OK) return ret;
    ret = ds3231_read_reg(REG_HOURS, &raw_hour);
    if (ret != ESP_OK) return ret;

    uint8_t cur_min  = bcd_to_dec(raw_min  & 0x7F); /* mascara bit 7 */
    uint8_t cur_hour = bcd_to_dec(raw_hour & 0x3F); /* mascara bits 6-7 */

    uint8_t tgt_min  = (cur_min + minutes) % 60;
    uint8_t tgt_hour = cur_hour + ((cur_min + minutes) / 60);
    tgt_hour         = tgt_hour % 24;

    ret  = ds3231_write_reg(REG_ALM1_SEC,  A1Mx_BIT);                /* A1M1=1 */
    ret |= ds3231_write_reg(REG_ALM1_MIN,  dec_to_bcd(tgt_min));     /* A1M2=0 */
    ret |= ds3231_write_reg(REG_ALM1_HOUR, dec_to_bcd(tgt_hour));    /* A1M3=0 */
    ret |= ds3231_write_reg(REG_ALM1_DAY,  A1Mx_BIT);                /* A1M4=1 */

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Alarm 1 programado para %02d:%02d", tgt_hour, tgt_min);
    }
    return ret;
}

esp_err_t ds3231_clear_alarm(void)
{
    if (s_dev == NULL) return ESP_ERR_INVALID_STATE;

    uint8_t status;
    esp_err_t ret = ds3231_read_reg(REG_STATUS, &status);
    if (ret != ESP_OK) return ret;

    return ds3231_write_reg(REG_STATUS, status & ~0x01);
}