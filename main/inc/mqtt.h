#ifndef MQTT_H
#define MQTT_H

#include <stdbool.h>
#include <stdint.h>

/* --- Configurações do broker MQTT com TLS --- */
#define MQTT_BROKER_HOST  "SEU_HOST"
#define MQTT_BROKER_PORT  00000 //SUA PORTA

/* Credenciais FIWARE */
#define FIWARE_API_KEY    "SUA_CHAVE_DE_API"
#define FIWARE_DEVICE_ID  "SEU_ID_DE_DISPOSITIVO"

/* Tópico: /<API_KEY>/<DEVICE_ID>/attrs */
#define MQTT_TOPIC_PUB    "/" FIWARE_API_KEY "/" FIWARE_DEVICE_ID "/attrs"

#define MQTT_CONNECTED_BIT  BIT0

/** @brief Inicializa e inicia o cliente MQTTS em background. */
void mqtt_app_start(void);

/**
 * @brief Bloqueia até o cliente MQTT estar conectado ao broker.
 * @param timeout_ms Tempo máximo de espera em milissegundos.
 * @return true se conectou no prazo, false se expirou.
 */
bool mqtt_wait_connected(uint32_t timeout_ms);

/**
 * @brief Publica no broker MQTT.
 * @return msg_id em caso de sucesso, -1 em erro.
 */
int mqtt_publish(const char *topic, const char *data, int qos);

#endif 