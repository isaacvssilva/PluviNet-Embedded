#ifndef MQTT_H
#define MQTT_H

#include <stdbool.h>
#include <stdint.h>
#include "sdkconfig.h"


#define MQTT_CONNECTED_BIT BIT0
 


#define MQTT_BROKER_HOST  CONFIG_MQTT_BROKER_HOST
#define MQTT_BROKER_PORT  CONFIG_MQTT_BROKER_PORT

#define FIWARE_API_KEY    CONFIG_FIWARE_API_KEY
#define FIWARE_DEVICE_ID  CONFIG_FIWARE_DEVICE_ID


#define MQTT_TOPIC_PUB    "/" FIWARE_API_KEY "/" FIWARE_DEVICE_ID "/attrs"
 
/**
 * @brief Inicializa e inicia o cliente MQTT em background (Task FreeRTOS).
 */
void mqtt_app_start(void);

/**
 * @brief Publica uma mensagem no broker MQTT.
 *
 * @param topic  Tópico de destino (string terminada em '\0').
 * @param data   Payload da mensagem (string terminada em '\0').
 * @param qos    Nível de qualidade de serviço: 0, 1 ou 2.
 * @return       msg_id da mensagem publicada, ou -1 em caso de erro.
 */
int mqtt_publish(const char *topic, const char *data, int qos);

/**
 * @brief Bloqueia até o cliente MQTT estar conectado ao broker.
 * @param timeout_ms Tempo máximo de espera em milissegundos.
 * @return true se conectou dentro do timeout, false caso contrário.
 */
bool mqtt_wait_connected(uint32_t timeout_ms);



#endif
