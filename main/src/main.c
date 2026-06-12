#include <stdio.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "driver/i2c_master.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "esp_rom_sys.h"


#include "wifi_provisioning/manager.h" 

#include "inc/sensor_hall.h"
#include "inc/sht30.h"
#include "inc/wifi.h"
#include "inc/mqtt.h"
#include "inc/ds3231.h"
#include "inc/uart.h"

#define TAG "PROV_PLUVINET"
#define SHT30_ADDR 0x44
#define MQTT_TIMEOUT_MS 15000
#define MQTT_ACK_DELAY_MS 2000
#define HALL_PULSE_TIMEOUT_US 300000UL
#define I2C_SDA_PIN 8
#define I2C_SCL_PIN 9

#define CONST_CALIBRACAO 0.5261

#define TEMPO_RESET_MS 3000 // 3 segundos

// Descomente para usar o RTC interno 
#define USE_INTERNAL_RTC

static i2c_master_bus_handle_t s_i2c_bus = NULL;

#define printf(...) uart_printf(__VA_ARGS__)

static void init_nvs(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
}

static i2c_master_bus_handle_t init_i2c(void)
{
    if (s_i2c_bus != NULL) {
        return s_i2c_bus;
    }

    i2c_master_bus_config_t bus_cfg = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_NUM_0,
        .sda_io_num = I2C_SDA_PIN,
        .scl_io_num = I2C_SCL_PIN,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_cfg, &s_i2c_bus));
    return s_i2c_bus;
}

#ifndef USE_INTERNAL_RTC
static void init_ds3231_gpio(void)
{
    gpio_hold_dis(DS3231_INT_PIN);

    gpio_config_t io = {
        .pin_bit_mask = (1ULL << DS3231_INT_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    ESP_ERROR_CHECK(gpio_config(&io));
    ESP_ERROR_CHECK(gpio_sleep_sel_dis(DS3231_INT_PIN));
}
#endif

static void configurar_wakeup(void)
{
#ifdef USE_INTERNAL_RTC
    ESP_ERROR_CHECK(esp_sleep_enable_timer_wakeup(600ULL * 1000000ULL));
    // Se usar RTC interno, o botão também precisa poder acordar a placa:
    ESP_ERROR_CHECK(esp_deep_sleep_enable_gpio_wakeup(
        (1ULL << PIN_BOTAO_RESET) | (1ULL << PIN_HALL), 
        ESP_GPIO_WAKEUP_GPIO_LOW));
#else
    ESP_ERROR_CHECK(
        esp_deep_sleep_enable_gpio_wakeup(
            (1ULL << PIN_HALL) | (1ULL << DS3231_INT_PIN) | (1ULL << PIN_BOTAO_RESET),
            ESP_GPIO_WAKEUP_GPIO_LOW));
#endif
}

static void armar_proximo_despertar(void)
{
#ifdef USE_INTERNAL_RTC
    ESP_LOGI(TAG, "Modo RTC interno: timer de 600s");
#else
    esp_err_t ret = ds3231_arm_alarm_1_minute();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao armar alarme DS3231: %s", esp_err_to_name(ret));
    }
#endif
}

static void verificar_botao_reset(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << PIN_BOTAO_RESET),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    ESP_ERROR_CHECK(gpio_config(&io_conf));

    // Se o botão não estiver pressionado (nível lógico HIGH), sai da função
    if (gpio_get_level(PIN_BOTAO_RESET) == 1) {
        return;
    }

    ESP_LOGW(TAG, "Botão de reset pressionado! Segure por 3 segundos para apagar o Wi-Fi...");
    
    uint32_t tempo_segurando = 0;
    while (gpio_get_level(PIN_BOTAO_RESET) == 0) { 
        esp_rom_delay_us(100000); // Espera 100ms
        tempo_segurando += 100;

        if (tempo_segurando >= TEMPO_RESET_MS) {
            ESP_LOGW(TAG, "============= FACTORY RESET =============");
            ESP_LOGW(TAG, "Apagando credenciais de Wi-Fi da NVS...");
            
           nvs_flash_erase();
            
            ESP_LOGW(TAG, "Credenciais apagadas! Reiniciando em 2 segundos...");
            vTaskDelay(pdMS_TO_TICKS(2000));
            esp_restart(); 
        }
    }
    ESP_LOGI(TAG, "Botão solto antes de 3 segundos. Reset cancelado.");
}

static void ciclo_completo(void)
{
    esp_sleep_wakeup_cause_t causa = esp_sleep_get_wakeup_cause();

    printf("\n================================================\n");
    if (causa == ESP_SLEEP_WAKEUP_TIMER) {
        printf("MOTIVO DO WAKEUP: [ TIMER INTERNO DA ESP32 ]\n");
    } else if (causa == ESP_SLEEP_WAKEUP_GPIO) {
        uint64_t gpio_status = esp_sleep_get_gpio_wakeup_status();
        if (gpio_status & (1ULL << DS3231_INT_PIN)) {
            printf("MOTIVO DO WAKEUP: [ RTC EXTERNO DS3231 ]\n");
        } else if (gpio_status & (1ULL << PIN_HALL)) {
            printf("MOTIVO DO WAKEUP: [ SENSOR HALL ]\n");
        } else if (gpio_status & (1ULL << PIN_BOTAO_RESET)) {
            printf("MOTIVO DO WAKEUP: [ BOTAO RESET (CANCELADO) ]\n");
        } else {
            printf("MOTIVO DO WAKEUP: [ GPIO ]\n");
        }
    } else {
        printf("MOTIVO DO WAKEUP: [ ENERGIA LIGADA / RESET ]\n");
    }
    printf("================================================\n\n");

    fflush(stdout);
    vTaskDelay(pdMS_TO_TICKS(100));

    uint32_t pulsos = sensor_hall_get_pulse_count();
    sensor_hall_set_pulse_count(0);
    ESP_LOGI(TAG, "Pulsos acumulados neste ciclo: %" PRIu32, pulsos);

    config_pin_hall();
    i2c_master_bus_handle_t i2c_bus = init_i2c();

#ifndef USE_INTERNAL_RTC
    init_ds3231_gpio();
    esp_err_t ret = ds3231_init(i2c_bus);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao inicializar DS3231: %s", esp_err_to_name(ret));
    } else {
        ret = ds3231_clear_alarm();
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "ds3231_clear_alarm falhou: %s", esp_err_to_name(ret));
        }
    }
#endif

    ESP_ERROR_CHECK(sht30_init(i2c_bus, SHT30_ADDR));

    wifi_init_prov();
    mqtt_app_start();

    ESP_LOGI(TAG, "Aguardando conexão MQTT...");
    if (mqtt_wait_connected(MQTT_TIMEOUT_MS)) {
        if (sht30_read() == ESP_OK) {
            char payload[128];
                snprintf(payload, sizeof(payload),
                     "t|%.2f#h|%.2f#r|%.2f",
                     sht30_get_temperatura(),
                     sht30_get_umidade(),
                     (float)pulsos * CONST_CALIBRACAO);
            ESP_LOGI(TAG, "-> FIWARE: %s", payload);
            printf("SERIAL PAYLOAD: %s\n", payload);
            mqtt_publish(MQTT_TOPIC_PUB, payload, 1);
            vTaskDelay(pdMS_TO_TICKS(MQTT_ACK_DELAY_MS));
        }
    }


    armar_proximo_despertar();
    configurar_wakeup();

    ESP_LOGI(TAG, "Entrando em Deep Sleep...");
    vTaskDelay(pdMS_TO_TICKS(200));
    fflush(stdout);
    esp_deep_sleep_start();
}

void app_main(void)
{
    vTaskDelay(pdMS_TO_TICKS(1500));

    init_nvs();

    verificar_botao_reset();

    config_pin_hall();
    uart_init();
    
    esp_sleep_wakeup_cause_t causa = esp_sleep_get_wakeup_cause();
    if (causa == ESP_SLEEP_WAKEUP_GPIO) {
        uint64_t gpio_status = esp_sleep_get_gpio_wakeup_status();

        if (gpio_status & (1ULL << PIN_HALL)) {
            sensor_hall_increment_pulse();
            gpio_set_direction(PIN_HALL, GPIO_MODE_INPUT);

            uint32_t t = HALL_PULSE_TIMEOUT_US;
            while (gpio_get_level(PIN_HALL) == 0 && t > 0) {
                esp_rom_delay_us(100);
                t -= 100;
            }

            esp_rom_delay_us(5000);
            configurar_wakeup();
            uart_wait_tx_done(UART_NUM_0, 100);
            esp_deep_sleep_start();
        }
    }

    // Se acordou por timer ou pelo botão (mas o usuário soltou antes de 3s), segue o fluxo normal
    ciclo_completo();
}