#include <avr/interrupt.h>

//void __attribute__((used, naked, section(".init8"))) init(void) {
//	PORTB.DIRSET = PIN2_bm;						// UART TX
//	USART0.BAUD = 116;							// 115200
//	USART0.CTRLB = USART_TXEN_bm | USART_RXEN_bm;
//}

static uint8_t size;
static char *pointer;

char uart_rxcif() { return USART0.STATUS & USART_RXCIF_bm; }

char uart_getc() { while (!uart_rxcif()); return USART0.RXDATAL; }

void uart_print(char *p, uint8_t s) {
	if (USART0.CTRLA & USART_DREIE_bm) return;
	pointer = p;
	size = s;
	USART0.CTRLA |= USART_DREIE_bm;
}

char *uart_print_string(char *p) {
	char *ptr = p;
	while (*++p);
	uart_print(ptr, p - ptr);
	return p;
}

ISR(USART0_DRE_vect) {
	USART0.TXDATAL = *pointer++;
	if (!--size) USART0.CTRLA &= ~USART_DREIE_bm;
}
