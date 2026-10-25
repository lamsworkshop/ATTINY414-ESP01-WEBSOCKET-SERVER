#include <avr/interrupt.h>
#include "uart.h"
#include "sha1.h"

/* ATTINY414 + ESP01 = websocket server
 * LED(PA1) ON when GOT IP
 * press BUTTON(PA6) before LED on to reset SSID/PASSWORD to default
 * set SSID/PASSWORD when received text string: AT+CWJAP="SSID","PASSWORD"\r\n
 * other text string echo back to client
 * unverified features: CLOSE and PING
 */

// PA6=button PA1=LED PA4=LEARN PA5=IR PA7=RST

typedef struct { uint8_t opcode, length; uint32_t mask, data[30]; } WS_FRAME;

extern struct { char message[97], accept[28]; uint32_t rnrn; } sram;
char cipsend[18] = "AT+CIPSEND=0,129\r\n";
#define ID cipsend[11]
enum { IDLE, TCP, WS } clients[4];
union { uint64_t w64; char byte[8]; uint32_t w32[2]; } window;
uint8_t send_length;

static const __flash char response[] =				// 97 characters
	"HTTP/1.1 101 Switching Protocols\r\n"
	"Upgrade: websocket\r\n"
	"Connection: Upgrade\r\n"
	"Sec-WebSocket-Accept: ";

void __attribute__((used, naked, section(".init8"))) init(void) {
	PORTB.DIRSET = PIN2_bm;						// UART TX
	USART0.BAUD = 116;							// 115200
	USART0.CTRLB = USART_TXEN_bm | USART_RXEN_bm;
	PORTA.OUTSET = PIN7_bm;						// ESP01-RST
	PORTA.DIRSET = PIN7_bm | PIN1_bm | PIN2_bm;	// RST, LEDs
	PORTA.PIN6CTRL = PORT_PULLUPEN_bm;			// button
}

void delayMS(unsigned i) {
	for (unsigned u = i << 8; u; u--) if (uart_rxcif()) break;
}

uint32_t shift_window() {
	window.w64 >>= 8; window.byte[7] = uart_getc();	// 0,CONNECT  0,CLOSED
	return window.w32[1];
}

char wait_token(int *id) {
	char tokens[7][8] = {
		"GOT IP\r\n", ",CONNECT", ",CLOSED\r", "\0\r\n+IPD,",
		"\0\0\0\0\r\n> ", "SEND OK\r", "DISCONNE"
	}; 
	while (1) {
		for (char key = PORTA.IN & PIN6_bm; !uart_rxcif(); delayMS(20))
			if (key != (PORTA.IN & PIN6_bm)) return key ? 7 : 8;
		shift_window();
		if (window.byte[7] == ',') *id = window.byte[6] & 3;
		for (char c = 0; c < 7; c++) {
			int i;
			for (i = 7; i >= 0; i--)
				if (window.byte[i] != tokens[c][i]) {
					if (tokens[c][i]) break;
					else return c;
				}
			if (i < 0) return c;
		}
	}
}

void set_tcp_server() {
	uart_print_string("AT+CIPMUX=1\r\n");
	while (shift_window() != 0x0A0D4B4F);			// OK\r\n
	uart_print_string("AT+CIPSERVER=1,9001\r\n");
	while (shift_window() != 0x0A0D4B4F);
	clients[0] = clients[1] = clients[2] = clients[3] = IDLE;
}

static char base64_c(uint8_t c) {
	if (c < 26) return c + 'A';
	if (c < 52) return c - 26 + 'a';
	if (c < 62) return c - 52 + '0';
	return c == 62 ? '+' : '/';
}

void base64(uint8_t *digest, char *output) {
	union {
		uint16_t word;
		struct { uint8_t low, high; };
	} data = { 0 };
	uint8_t bits = 0;
	for (uint8_t i = 0; i < 21; i++) {
		data.low = digest[(i | 3) - (i % 4)];
		bits += 8;
		do {
			data.word <<= bits > 6 ? 14 - bits : bits;
			if (bits < 6) break;
			*output++ = base64_c(data.high);
			data.high = 0;
		} while (bits -= 6);
	}
}

static void send_message(uint8_t length) {
	uint8_t idx = 13;
	if ((send_length = length) > 99) { length -= 100; cipsend[idx++] = '1'; }
	cipsend[idx] = '0';
	while (length > 9) { cipsend[idx]++; length -= 10; }
	if ((idx != 13)||(cipsend[idx] != '0')) idx++;
	cipsend[idx++] = length + '0';
	cipsend[idx++] = '\r';
	cipsend[idx++] = '\n';
	uart_print(cipsend, idx);
}

void ipd_tcp(unsigned length) {
	int j = 0;
	for (int i = 0; length; length--)
		if (!j) {
			shift_window();
			if (window.w64 == 0x203A79654B2D7465) j++;
		} else sram.message[i > 23 ? 24 : i++] = uart_getc();
	if (j) {
		base64(sha1(), sram.accept);
		mem_copy(sram.message, response, 97);
		sram.accept[27] = '=';
		sram.rnrn = 0x0A0D0A0D;
		send_message(129);
	}
}

void ipd_ws(unsigned length, WS_FRAME *frame) {
	for (uint8_t i = 0; length; length--) sram.message[i++] = uart_getc();
	frame->length &= 127;				// length must be multiple of 4
	for (int i = 0; i < frame->length; i += 4)
		frame->data[i >> 2] ^= frame->mask;
	for (int i = 0; i < 30; i++)			// move data up to cover mask
		frame->data[i - 1] = frame->data[i];
	switch (frame->opcode) {
	case 0x88:	clients[ID & 3] = TCP; break;			// CLOSE
	case 0x89:	frame->opcode = 0x8A; break;			// PING
	case 0x81:											// TEXT
				if (*(uint64_t*)&frame->mask == 0x50414A57432B5441)
					return uart_print((char*)&frame->mask, frame->length);
				break;
	default:	return;									// BINARY
	}
	send_message(frame->length + 2);
}

void ipd() {
	unsigned length = 0;
	ID = uart_getc(); uart_getc();		// +IPD,{id},{length}:
	for (char c; (c = uart_getc()) != ':'; length = length * 10 + c - '0');
	switch (clients[ID & 3]) {
	case TCP: return ipd_tcp(length);
	case WS: return ipd_ws(length, (WS_FRAME*)sram.message);
	default: while (length--) uart_getc();		// drain all left
	}
}

int main() {
	delayMS(500);
	PORTA.OUTCLR = PIN7_bm;		// reset pulse
	delayMS(10);
	PORTA.OUTSET = PIN7_bm;
	delayMS(500);
	sei();
	while (1) {
		int id;
		switch (wait_token(&id)) {
		case 0:	set_tcp_server();								// GOT IP
				PORTA.OUTSET = PIN1_bm;
				break;
		case 1:	clients[id] = TCP; break;						// CONNECT
		case 2: clients[id] = IDLE; break;						// CLOSED
		case 3:	ipd(); break;									// +IPD,
		case 4:	uart_print(sram.message, send_length); break;	// PROMPT >
		case 5:	clients[ID & 3] = WS; break;					// SEND OK
		case 6:	PORTA.OUTCLR = PIN1_bm; break;					// DISCONNECT
		case 7:	break;											// key down
		case 8:	if (!(PORTA.IN & PIN1_bm)) uart_print_string(	// key up
					"AT+CWJAP=\"SSID\",\"PASSWORD\"\r\n"
				);
		}
	}
}
