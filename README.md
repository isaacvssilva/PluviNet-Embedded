# PluviNet: Estação Meteorológica e Pluviômetro IoT 

O **PluviNet** é um protótipo de sistema embarcado de baixo consumo e baixo custo projetado para o monitoramento meteorológico automatizado. Utilizando um microcontrolador ESP32-C3, o dispositivo coleta dados de precipitação pluviométrica, temperatura e umidade relativa do ar. Periodicamente, os dados são transmitidos para uma plataforma IoT baseada em FIWARE através do protocolo MQTT.

## Principais Funcionalidades

* **Ultra Baixo Consumo (Deep Sleep):** O sistema foi desenhado para ser alimentado por bateria e painel solar. Ele passa entre 85% e 90% do tempo em Deep Sleep, reduzindo o consumo para a faixa de 5-10 µA.
* **Quick Wakeup (Interrupção por Hardware):** O sensor pluviométrico de báscula utiliza um sensor magnético de efeito Hall. A cada basculamento, um pulso acorda a CPU em microssegundos apenas para incrementar o contador na *RTC Slow Memory*, retornando imediatamente ao Deep Sleep.
* **Provisionamento Wi-Fi via BLE:** Permite a configuração dinâmica da rede Wi-Fi via smartphone, eliminando a necessidade de credenciais "hardcoded" no firmware.
* **Otimização Crítica de Memória:** O sistema libera automaticamente toda a memória RAM utilizada pelo stack Bluetooth (NimBLE) logo após o provisionamento de rede.
* **Comunicação Segura e Integração FIWARE:** A comunicação com o broker Mosquitto é protegida por mTLS (TLS mútuo). O payload segue o protocolo UltraLight 2.0 (ex: `t|<temperatura>#h|<umidade>#r|<rain>`) e é publicado nativamente no ecossistema FIWARE.

## Hardware e Pinout

O esquemático do projeto utiliza o **ESP32-C3 Super Mini** (arquitetura RISC-V) e módulos externos interconectados.

**Componentes:**
* Microcontrolador: ESP32-C3 Super Mini (3,3 V)
* Módulo RTC: DS3231 (Comunicação I2C)
* Sensor de Temperatura e Umidade: SHT30 (Comunicação I2C)
* Sensor de Efeito Hall: A1104xLH

**Mapeamento de Pinos (GPIO):**
* `GPIO3`: Botão de Reset de Fábrica (Pull-up 10KΩ)
* `GPIO4`: Interrupção do Alarme DS3231 (Pull-up 4,7KΩ)
* `GPIO5`: Sinal do Sensor Hall (Pull-up 10KΩ)
* `GPIO8`: Barramento I2C - SDA (Pull-up 4,7KΩ)
* `GPIO9`: Barramento I2C - SCL (Pull-up 4,7KΩ)
* `GPIO20`: Debug UART TX

## ⚙️ Instalação e Configuração

O código-fonte foi desenvolvido utilizando o framework oficial **ESP-IDF** (baseado em FreeRTOS).

### 1. Preparando o Ambiente
Clone o repositório para a sua máquina local:
```bash
git clone [https://github.com/isaacvssilva/PluviNet-Embedded.git](https://github.com/isaacvssilva/PluviNet-Embedded.git)
cd PluviNet-Embedded