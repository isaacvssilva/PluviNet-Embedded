/*
 * Diagrama de conexão:
 *   DS3231 SDA  → GPIO 8  
 *   DS3231 SCL  → GPIO 9  
 *   DS3231 INT  → GPIO 4  (DS3231_INT_PIN, pull-up externo 10 kΩ)
 *   DS3231 VCC  → 3.3 V
 *   DS3231 GND  → GND
 *
 */

/* Seleciona modo de temporização */
#define USE_INTERNAL_RTC
 
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
 
#include "inc/sensor_hall.h"
#include "inc/sht30.h"
#include "inc/wifi.h"
#include "inc/mqtt.h"
 
#ifndef USE_INTERNAL_RTC
#include "inc/ds3231.h"
#endif


#define TAG               "PROV_PLUVINET"
#define SHT30_ADDR        0x44
 
#ifdef USE_INTERNAL_RTC
  #define SLEEP_SECONDS   60
  #define SLEEP_US        ((uint64_t)SLEEP_SECONDS * 1000000ULL)
#else
  #define ALARM_MINUTES   1
#endif
 
#define MQTT_TIMEOUT_MS   15000
#define MQTT_ACK_DELAY_MS  2000

/*
 * Máximo tempo que aguardamos o pino do Hall voltar ao nível alto
 * após ser incrementado. Evita loop infinito se o hardware travar.
 */
#define HALL_PULSE_TIMEOUT_US  300000UL


//nvs Non volatile storage
static void init_nvs(void)
{
    esp_err_t ret = nvs_flash_init(); //tenta iniciar a memoria nvs
    // verifica se ocorreu erro por falta de espaço
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase()); //se houve erro ele apaga a memoria nvs e tenta inicia-la novamente
        ret = nvs_flash_init(); //tenta reiniciar a esp caso a inicialização da memoria falhe
    }
    ESP_ERROR_CHECK(ret);
}

static i2c_master_bus_handle_t init_i2c(void)
{ //struct para configurar o I2C
    i2c_master_bus_config_t bus_cfg = {
        .clk_source                   = I2C_CLK_SRC_DEFAULT, //define a fonte de clock padrão
        .i2c_port                     = I2C_NUM_0,           //define a porta
        .sda_io_num                   = I2C_SDA_PIN,         //pinos de dados sda
        .scl_io_num                   = I2C_SCL_PIN,         //pinos de dados scl
        .glitch_ignore_cnt            = 7,                   //ignora pequenos ruidos
        .flags.enable_internal_pullup = true,                // ativa resistores internos de pullup
    };
    i2c_master_bus_handle_t bus;  //cria a variavel para representar o barramento
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_cfg, &bus)); //aplica as config no hardware
    return bus; //retorna a referencia do i2c configurado 
}

/*
 * Configura o GPIO do pino INT do DS3231 como entrada com pull-up.
 * O DS3231 puxa o pino para LOW quando o alarme dispara.
 * O ESP32-C3 acorda quando detecta GPIO LOW no modo deep sleep.
 */
#ifndef USE_INTERNAL_RTC
/*
 * Configura o GPIO do pino INT do DS3231 como entrada com pull-up.
 */
static void init_ds3231_gpio(void)
{
    gpio_config_t io = {
        .pin_bit_mask = (1ULL << DS3231_INT_PIN),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    gpio_config(&io);
}
#endif


static void configurar_wakeup(void)
{
#ifdef USE_INTERNAL_RTC
    esp_sleep_enable_timer_wakeup(SLEEP_US);
 
#else
    ESP_ERROR_CHECK(
        esp_deep_sleep_enable_gpio_wakeup(
            (1ULL << PIN_HALL) | (1ULL << DS3231_INT_PIN),
            ESP_GPIO_WAKEUP_GPIO_LOW));
#endif
 
#ifdef USE_INTERNAL_RTC
    ESP_ERROR_CHECK(
        esp_deep_sleep_enable_gpio_wakeup(
            1ULL << PIN_HALL,
            ESP_GPIO_WAKEUP_GPIO_LOW));
#endif
}



static void ciclo_completo(void)
{
    esp_sleep_wakeup_cause_t causa = esp_sleep_get_wakeup_cause();
 
    /* Log do motivo */
    switch (causa) {
    case ESP_SLEEP_WAKEUP_TIMER:
        ESP_LOGI(TAG, "Acordei: timer RTC interno");  break;
    case ESP_SLEEP_WAKEUP_GPIO:
        ESP_LOGI(TAG, "Acordei: DS3231 (GPIO4)");     break;
    default:
        ESP_LOGI(TAG, "Boot inicial ou reset");        break;
    }
 
    /*Lê e zera o contador de pulsos acumulado pelos quick wakeups*/
    uint32_t pulsos = sensor_hall_get_pulse_count();
    sensor_hall_set_pulse_count(0);
    ESP_LOGI(TAG, "Pulsos acumulados neste ciclo: %" PRIu32, pulsos);
 
    /*Hardware*/
    init_nvs();
    config_pin_hall();   /* reconfigura GPIO5*/
    i2c_master_bus_handle_t i2c_bus = init_i2c();
    ESP_ERROR_CHECK(sht30_init(i2c_bus, SHT30_ADDR));
 
#ifndef USE_INTERNAL_RTC
    init_ds3231_gpio(); 
    ESP_ERROR_CHECK(ds3231_init(i2c_bus));
    /*
     * Limpa o flag A1F imediatamente após acordar.
     * Se não limpar, o pino INT permanece em LOW e o próximo
     * alarme não consegue sinalizar nova borda.
     */
    ESP_ERROR_CHECK(ds3231_clear_alarm());
#endif
 
    /*Rede e publicação*/
    wifi_init_prov();
    mqtt_app_start();
 
    if (!mqtt_wait_connected(MQTT_TIMEOUT_MS)) {
        ESP_LOGE(TAG, "Timeout MQTT — publicação pulada neste ciclo");
        goto sleep;
    }
 
    if (sht30_read() == ESP_OK) {
        char payload[128];
        snprintf(payload, sizeof(payload),
                 "t|%.2f#h|%.2f#p|%" PRIu32,
                 sht30_get_temperatura(),
                 sht30_get_umidade(),
                 pulsos);
 
        ESP_LOGI(TAG, "→ FIWARE: %s", payload);
        mqtt_publish(MQTT_TOPIC_PUB, payload, 1);
 
        /* Aguarda ACK (QoS 1) antes de desligar o rádio */
        vTaskDelay(pdMS_TO_TICKS(MQTT_ACK_DELAY_MS));
    } else {
        ESP_LOGE(TAG, "Falha na leitura SHT30");
    }
 
sleep:
    /*Configura wakeup e dorme*/
#ifndef USE_INTERNAL_RTC
    ESP_ERROR_CHECK(ds3231_arm_alarm(ALARM_MINUTES)); 
#endif
    configurar_wakeup();
    esp_deep_sleep_start();
    /* Nunca chega aqui */
}





void app_main(void)
{

    /* Identifica o motivo do acordar para log */
    esp_sleep_wakeup_cause_t causa = esp_sleep_get_wakeup_cause();
    if (causa == ESP_SLEEP_WAKEUP_GPIO) {
        uint64_t gpio_status = esp_sleep_get_gpio_wakeup_status();
 
        if (gpio_status & (1ULL << PIN_HALL)) {
 
            sensor_hall_increment_pulse();
            // Reconfigura rapidamente o pino apenas para leitura
            gpio_set_direction(PIN_HALL, GPIO_MODE_INPUT);
            /*
             * Aguarda o pino retornar ao nível HIGH (fim do pulso).
             * Se dormirmos com GPIO5 ainda em LOW, o chip acorda
             * imediatamente de novo
             */
            uint32_t t = HALL_PULSE_TIMEOUT_US;
            while (gpio_get_level(PIN_HALL) == 0 && t > 0) {
                esp_rom_delay_us(100);
                t -= 100;
            }
            /* Debounce adicional de 5 ms */
            esp_rom_delay_us(5000);
 
            /* Re-arma fontes de wakeup e dorme */
            configurar_wakeup();
            esp_deep_sleep_start();
 
            /* Nunca chega aqui */
        }
 
        /*
         * GPIO4 (DS3231) acordou o chip → cai no ciclo completo abaixo.
         */
    }

    ciclo_completo();
    
}