#include "inc/ds3231.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "DS3231";

#define REG_SECONDS   0x00
#define REG_MINUTES   0x01
#define REG_HOURS     0x02
#define REG_ALM1_SEC  0x07
#define REG_ALM1_MIN  0x08
#define REG_ALM1_HOUR 0x09
#define REG_ALM1_DAY  0x0A
#define REG_CONTROL   0x0E
#define REG_STATUS    0x0F
#define A1Mx_BIT      (1 << 7)

static i2c_master_dev_handle_t s_dev = NULL;

static uint8_t bcd_to_dec(uint8_t v) { return (v >> 4) * 10 + (v & 0x0F); }
static uint8_t dec_to_bcd(uint8_t v) { return ((v / 10) << 4) | (v % 10); }

static esp_err_t ds3231_write_reg(uint8_t reg, uint8_t value)
{
    if (s_dev == NULL) return ESP_ERR_INVALID_STATE;
    uint8_t data[2] = {reg, value};
    return i2c_master_transmit(s_dev, data, sizeof(data), pdMS_TO_TICKS(100));
}

static esp_err_t ds3231_read_reg(uint8_t reg, uint8_t *out)
{
    if (s_dev == NULL) return ESP_ERR_INVALID_STATE;
    return i2c_master_transmit_receive(s_dev, &reg, 1, out, 1, pdMS_TO_TICKS(100));
}

static esp_err_t ds3231_read_regs(uint8_t reg, uint8_t *buf, size_t len)
{
    if (s_dev == NULL) return ESP_ERR_INVALID_STATE;
    return i2c_master_transmit_receive(s_dev, &reg, 1, buf, len, pdMS_TO_TICKS(100));
}

esp_err_t ds3231_init(i2c_master_bus_handle_t bus)
{
    if (s_dev != NULL) {
        return ESP_OK;
    }

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = DS3231_I2C_ADDR,
        .scl_speed_hz = 100000,
        .scl_wait_us = 0,
        .flags.disable_ack_check = false,
    };

    esp_err_t ret = i2c_master_bus_add_device(bus, &dev_cfg, &s_dev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao adicionar DS3231 no barramento I2C: %s", esp_err_to_name(ret));
        s_dev = NULL;
        return ret;
    }

    uint8_t control, status;
    ret = ds3231_read_reg(REG_CONTROL, &control);
    if (ret != ESP_OK) return ret;
    ret = ds3231_read_reg(REG_STATUS, &status);
    if (ret != ESP_OK) return ret;

    control &= ~(1 << 3);
    control |=  (1 << 2);
    control |=  (1 << 0);
    control &= ~(1 << 1);
    status  &= ~(1 << 0);
    status  &= ~(1 << 1);

    ret = ds3231_write_reg(REG_CONTROL, control);
    if (ret != ESP_OK) return ret;
    ret = ds3231_write_reg(REG_STATUS, status);
    if (ret != ESP_OK) return ret;

    ESP_LOGI(TAG, "DS3231 iniciado: INTCN=1, A1IE=1, flags limpas");
    return ESP_OK;
}

esp_err_t ds3231_clear_alarm(void)
{
    uint8_t status;
    esp_err_t ret = ds3231_read_reg(REG_STATUS, &status);
    if (ret != ESP_OK) return ret;
    return ds3231_write_reg(REG_STATUS, status & ~0x01);
}

esp_err_t ds3231_arm_alarm_10_seconds(void)
{
    esp_err_t ret = ds3231_clear_alarm();
    if (ret != ESP_OK) return ret;

    uint8_t now[3];
    ret = ds3231_read_regs(REG_SECONDS, now, 3);
    if (ret != ESP_OK) return ret;

    uint8_t sec  = bcd_to_dec(now[0] & 0x7F);
    uint8_t min  = bcd_to_dec(now[1] & 0x7F);
    uint8_t hour = bcd_to_dec(now[2] & 0x3F);

    sec += 10;
    if (sec >= 60) {
        sec -= 60;
        min++;
    }
    if (min >= 60) {
        min = 0;
        hour = (hour + 1) % 24;
    }

    ret  = ds3231_write_reg(REG_ALM1_SEC, dec_to_bcd(sec) & 0x7F);
    ret |= ds3231_write_reg(REG_ALM1_MIN, dec_to_bcd(min) & 0x7F);
    ret |= ds3231_write_reg(REG_ALM1_HOUR, dec_to_bcd(hour) & 0x7F);
    ret |= ds3231_write_reg(REG_ALM1_DAY, A1Mx_BIT);

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Alarm 1 programado para %02u:%02u:%02u (+10s)", hour, min, sec);
    }
    return ret;
}


esp_err_t ds3231_arm_alarm_1_minute(void)
{
    if (s_dev == NULL) return ESP_ERR_INVALID_STATE;

    esp_err_t ret = ds3231_clear_alarm();
    if (ret != ESP_OK) return ret;

    uint8_t now[3];
    ret = ds3231_read_regs(REG_SECONDS, now, 3);
    if (ret != ESP_OK) return ret;

    uint8_t sec  = bcd_to_dec(now[0] & 0x7F);
    uint8_t min  = bcd_to_dec(now[1] & 0x7F);
    uint8_t hour = bcd_to_dec(now[2] & 0x3F);

    min += 1;
    if (min >= 60) {
        min = 0;
        hour = (hour + 1) % 24;
    }

    ret  = ds3231_write_reg(REG_ALM1_SEC,  dec_to_bcd(sec) & 0x7F);
    ret |= ds3231_write_reg(REG_ALM1_MIN,  dec_to_bcd(min) & 0x7F);
    ret |= ds3231_write_reg(REG_ALM1_HOUR, dec_to_bcd(hour) & 0x7F);
    ret |= ds3231_write_reg(REG_ALM1_DAY,  A1Mx_BIT);

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Alarm 1 programado para %02u:%02u:%02u (+1 min)",
                 hour, min, sec);
    }
    return ret;
}
