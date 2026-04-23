#include "inc/mqtt.h"

#include <stdio.h>
#include "esp_log.h"
#include "mqtt_client.h"

static const char *TAG = "MQTT";

 

static esp_mqtt_client_handle_t s_mqtt_client    = NULL;
static EventGroupHandle_t       s_mqtt_event_grp = NULL;


/**
 * @brief Gerencia eventos do ciclo de vida do protocolo MQTT.
 */
static void mqtt_event_handler(void *handler_args, esp_event_base_t base,
                                int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;

    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "Conectado ao broker. Tópico padrão: %s", MQTT_TOPIC_PUB);
        break;

    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "Desconectado. Tentando reconectar...");
        break;

    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "Publicação confirmada (Msg ID=%d)", event->msg_id);
        break;

    case MQTT_EVENT_ERROR:
        ESP_LOGE(TAG, "Erro MQTT detectado");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            ESP_LOGE(TAG, "Erro TCP (Socket errno=%d). Verifique rede/firewall.",
                     event->error_handle->esp_transport_sock_errno);
        }
        break;

    default:
        break;
    }
}

void mqtt_app_start(void)
{
    char uri_buffer[64];
    snprintf(uri_buffer, sizeof(uri_buffer), "mqtt://%s:%d",
             MQTT_BROKER_HOST, MQTT_BROKER_PORT);

    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri    = uri_buffer,
        .credentials.client_id = FIWARE_DEVICE_ID,
        .credentials.username  = FIWARE_API_KEY,
    };

    s_mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(s_mqtt_client, ESP_EVENT_ANY_ID,
                                   mqtt_event_handler, NULL);
    esp_mqtt_client_start(s_mqtt_client);
}

int mqtt_publish(const char *topic, const char *data, int qos)
{
    if (s_mqtt_client == NULL) {
        ESP_LOGE(TAG, "mqtt_publish chamado antes de mqtt_app_start.");
        return -1;
    }
    return esp_mqtt_client_publish(s_mqtt_client, topic, data, 0, qos, 0);
}

bool mqtt_wait_connected(uint32_t timeout_ms)
{
    if (s_mqtt_event_grp == NULL) return false;
    EventBits_t bits = xEventGroupWaitBits(
        s_mqtt_event_grp, MQTT_CONNECTED_BIT,
        pdFALSE, pdTRUE, pdMS_TO_TICKS(timeout_ms));
    return (bits & MQTT_CONNECTED_BIT) != 0;
}
