#ifndef UART_H
#define UART_H

#include "driver/uart.h" // Isso resolve o erro do 'uart_port_t'
#define UART_TX_PIN 20
#define UART_RX_PIN 21
// Declarações das funções
void uart_init(void);
void uart_puts(uart_port_t uart_num, const char *s);
void uart_printf(const char *fmt, ...); 

#endif