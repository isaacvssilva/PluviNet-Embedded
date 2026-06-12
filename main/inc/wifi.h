#ifndef WIFI_H
#define WIFI_H

/**
 * @brief Bit de status da conexão Wi-Fi no EventGroup do FreeRTOS.
 * Setado quando o IP é obtido com sucesso.
 */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1
#define WIFI_MAXIMUM_RETRY 5

#define PIN_BOTAO_RESET 3
#define TEMPO_RESET_MS 3000

/**
 * @brief Inicializa Wi-Fi com Provisionamento Inteligente via BLE.
 */
void wifi_init_prov(void);


#endif 