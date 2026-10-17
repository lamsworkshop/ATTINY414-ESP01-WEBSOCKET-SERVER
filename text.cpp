#include <iostream>
#include <cstring>
#include <cstdint>
#include <random>

using namespace std;

struct __attribute__((packed)) {
	uint8_t opcode, length;
	uint32_t mask;
	union { uint32_t word[30]; char byte[120]; };
} frame;

int main(int argc, char* argv[]) {
	random_device rd;
	mt19937 gen(rd());
	frame.mask = gen();
	frame.opcode = 0x81;
	frame.length = strlen(argv[1]);
	int i;
	for (i = 0; i < frame.length; i++) frame.byte[i] = argv[1][i];
	frame.byte[i++] = '\r';
	frame.byte[i] = '\n';
	frame.length += 2;
	for (i = 0; i < frame.length; i += 4) frame.word[i>>2] ^= frame.mask;
	int length = frame.length + 6;
	frame.length |= 0x80;
	cout << "+IPD,0," << length << ':';
	char *p = (char*)&frame;
	for (int i = 0; i < length; i++) cout << *p++;
	return 0;
}
