#include "inc/uart.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h> 
#include "driver/gpio.h"
#include "driver/uart.h"


void uart_init(void) {
    if (uart_is_driver_installed(UART_NUM_0)) return;
    
    const int uart_buffer_size = (1024 * 2);
    QueueHandle_t uart_queue;
    
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    uart_driver_install(UART_NUM_0, uart_buffer_size, uart_buffer_size, 10, &uart_queue, 0);
    uart_param_config(UART_NUM_0, &uart_config);
    uart_set_pin(UART_NUM_0, UART_TX_PIN, UART_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
}

void uart_puts(uart_port_t uart_num, const char *s) {
    uart_write_bytes(uart_num, (const char*)s, strlen(s));
}



void uart_printf(const char *fmt, ...) {
    char buffer[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    
    uart_puts(UART_NUM_0, buffer);
}