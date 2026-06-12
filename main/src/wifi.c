#include "inc/wifi.h"

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "wifi_provisioning/manager.h"
#include "wifi_provisioning/scheme_ble.h" 
#include "mqtt_client.h"


static const char *TAG = "PROV_PLUVINET";

/**
 * @brief Handle para o Grupo de Eventos do FreeRTOS.
 * Utilizado para sincronizar a tarefa principal com o estado da conexão Wi-Fi.
 */
static EventGroupHandle_t s_wifi_event_group;

static int s_retry_num = 0; // Contador de tentativas de conexão

/**
 * @brief Manipulador centralizado de eventos (Event Handler).
 * * Trata eventos de três fontes distintas:
 * 1. WIFI_EVENT: Mudanças no estado físico do Wi-Fi.
 * 2. IP_EVENT: Aquisição de endereço IP.
 * 3. WIFI_PROV_EVENT: Estados do processo de provisionamento.
 * * @param arg Argumentos do usuário (não utilizado).
 * @param event_base Base do evento.
 * @param event_id ID do evento específico.
 * @param event_data Dados associados ao evento.
 */
static void event_handler(void* arg, esp_event_base_t event_base,
                          int32_t event_id, void* event_data)
{
    /* Verificando eventos de infraestrutura Wi-Fi */
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();

    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < WIFI_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "Tentando reconectar ao Ponto de Acesso... (%d/%d)", s_retry_num, WIFI_MAXIMUM_RETRY);
        } else {
            ESP_LOGE(TAG, "Falha ao conectar após %d tentativas. Desistindo.", WIFI_MAXIMUM_RETRY);
            /* Sinaliza falha para desbloquear a tarefa principal e não travar o sistema */
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        
        /* Limpando o bit de conexão para bloquear a tarefa principal */
        xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
    /* Verificando a obtenção de IP */
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Conectado! IP: " IPSTR, IP2STR(&event->ip_info.ip));

        s_retry_num = 0;

        /* Sinalizando sucesso para desbloquear o app_main */
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    } 
    /* Gerenciando o fluxo de Provisionamento */
    else if (event_base == WIFI_PROV_EVENT) {
        switch (event_id) {
            case WIFI_PROV_START:
                ESP_LOGI(TAG, "Provisioning iniciado via BLE (Aguardando App)...");
                break;
            case WIFI_PROV_CRED_SUCCESS:
                ESP_LOGI(TAG, "Provisioning realizado com sucesso!");
                break;
            case WIFI_PROV_END:
                /* Liberando recursos de memória do Manager e do Bluetooth */
                wifi_prov_mgr_deinit(); 
                break;
            default: 
                break;
        }
    }
}

/**
 * @brief Inicializa a infraestrutura de rede com Provisionamento Inteligente.
 * * Lógica de operação:
 * 1. Inicializa a pilha TCP/IP e Wi-Fi.
 * 2. Verifica se existem credenciais salvas na NVS.
 * 3. Se NÃO houver: Inicia o BLE e aguarda configuração pelo App.
 * 4. Se HOUVER: Conecta diretamente ao Wi-Fi, economizando tempo e energia.
 */
void wifi_init_prov(void)
{
    /* Inicializando a camada de rede (Netif) e o loop de eventos */
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    
    /* Criando o grupo de eventos e a interface Station padrão */
    s_wifi_event_group = xEventGroupCreate();
    esp_netif_create_default_wifi_sta();

    /* Inicializando o driver Wi-Fi com configuração padrão */
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    /* Registrando os manipuladores para eventos de Provisionamento, Wi-Fi e IP */
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_PROV_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL));

    /* Configurando o Gerenciador de Provisionamento com esquema BLE (NimBLE) */
    bool provisioned = false;
    wifi_prov_mgr_config_t config = {
        .scheme = wifi_prov_scheme_ble,
        /* Definindo flag crítica para liberar memória BTDM após o uso */
        .scheme_event_handler = WIFI_PROV_SCHEME_BLE_EVENT_HANDLER_FREE_BTDM
    };

    /* Inicializando o manager e verificando estado da NVS */
    ESP_ERROR_CHECK(wifi_prov_mgr_init(config));
    ESP_ERROR_CHECK(wifi_prov_mgr_is_provisioned(&provisioned)); 

    if (!provisioned) {
        ESP_LOGI(TAG, "Sem senha salva. Iniciando BLE para configuração...");
        
        /* Iniciando o serviço de provisionamento com Segurança Nível 1 (Proof of Possession) */
        ESP_ERROR_CHECK(wifi_prov_mgr_start_provisioning(WIFI_PROV_SECURITY_1, "123456", "PROV_PLUVINET", NULL));
    } else {
        ESP_LOGI(TAG, "Senha encontrada na NVS! Conectando diretamente...");
        
        /* Desalocando o manager pois as credenciais já estão salvas */
        wifi_prov_mgr_deinit();
        
        /* Iniciando o Wi-Fi Station manualmente */
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_start());
    }

    /* Aguardando a conexão ser estabelecida OU o limite de tentativas falhar */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT, /* Aguarda um dos dois bits */
            pdFALSE,
            pdFALSE, /* pdFALSE = Não exige que ambos sejam verdadeiros */
            portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Wi-Fi estabelecido com sucesso!");
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGW(TAG, "O Wi-Fi falhou e não conseguiu conectar. O sistema prosseguirá sem rede.");
    } else {
        ESP_LOGE(TAG, "Erro inesperado no EventGroup do Wi-Fi");
    }
}
