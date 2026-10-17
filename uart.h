#ifndef UART_H
#define UART_H

char uart_rxcif();
char uart_getc();
void uart_print(char*, uint8_t);
char *uart_print_string(char*);

#endif

