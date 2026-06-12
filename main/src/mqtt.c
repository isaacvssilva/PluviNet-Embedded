#include "inc/mqtt.h"

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "mqtt_client.h"

static const char *TAG = "MQTT";

static esp_mqtt_client_handle_t s_client    = NULL;
static EventGroupHandle_t       s_evt_grp   = NULL;

extern const uint8_t ca_pem_start[]   asm("_binary_ca_pem_start");
extern const uint8_t ca_pem_end[]     asm("_binary_ca_pem_end");
extern const uint8_t cert_crt_start[] asm("_binary_cert_crt_start");
extern const uint8_t cert_crt_end[]   asm("_binary_cert_crt_end");
extern const uint8_t cert_key_start[] asm("_binary_cert_key_start");
extern const uint8_t cert_key_end[]   asm("_binary_cert_key_end");


static void mqtt_event_handler(void *handler_args, esp_event_base_t base,
                                int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;

    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "Conectado ao broker (TLS). Tópico: %s", MQTT_TOPIC_PUB);
        xEventGroupSetBits(s_evt_grp, MQTT_CONNECTED_BIT);
        break;

    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "Desconectado. Tentando reconectar...");
        xEventGroupClearBits(s_evt_grp, MQTT_CONNECTED_BIT);
        break;

    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "Publicação confirmada (Msg ID=%d)", event->msg_id);
        break;

    case MQTT_EVENT_ERROR:
        ESP_LOGE(TAG, "Erro MQTT detectado");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            ESP_LOGE(TAG, "Erro TLS/TCP (errno=%d)",
                     event->error_handle->esp_transport_sock_errno);
        }
        break;

    default:
        break;
    }
}


void mqtt_app_start(void)
{
    s_evt_grp = xEventGroupCreate();


    esp_mqtt_client_config_t cfg = {
        /* Configuração de Endereço */
        .broker.address.hostname = MQTT_BROKER_HOST,
        .broker.address.port = MQTT_BROKER_PORT,
        .broker.address.transport = MQTT_TRANSPORT_OVER_SSL, /* Força o uso de TLS */

        .network.timeout_ms = 60000,

        /* Autenticação do servidor (CA) */
        .broker.verification.certificate = (const char *)ca_pem_start,

        /* Autenticação mTLS do dispositivo  */
        .credentials.authentication.certificate = (const char *)cert_crt_start,
        .credentials.authentication.key         = (const char *)cert_key_start,

        /* Identidade e Credenciais  */
        .credentials.client_id = FIWARE_DEVICE_ID,
        .credentials.username  = "teste", 
        .credentials.authentication.password = "teste", 
    };

    s_client = esp_mqtt_client_init(&cfg);
    esp_mqtt_client_register_event(s_client, ESP_EVENT_ANY_ID,
                                   mqtt_event_handler, NULL);
    esp_mqtt_client_start(s_client);
}

bool mqtt_wait_connected(uint32_t timeout_ms)
{
    if (!s_evt_grp) return false;
    EventBits_t bits = xEventGroupWaitBits(
        s_evt_grp, MQTT_CONNECTED_BIT,
        pdFALSE, pdTRUE, pdMS_TO_TICKS(timeout_ms));
    return (bits & MQTT_CONNECTED_BIT) != 0;
}

int mqtt_publish(const char *topic, const char *data, int qos)
{
    if (!s_client) {
        ESP_LOGE(TAG, "mqtt_publish chamado antes de mqtt_app_start");
        return -1;
    }
    return esp_mqtt_client_publish(s_client, topic, data, 0, qos, 0);
}