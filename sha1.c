#include "sha1.h"

struct __attribute__((packed)) {
	uint8_t buffer[64]; uint32_t state[5], count; char x[41];
} sram;

void mem_copy(char *d, const __flash char *s, uint8_t size) {
	for (int i = 0; i < size; i++) d[i] = s[i];
}

#define LEFT_ROTATE(value, bits) (((value) << (bits)) | ((value) >> (32 - (bits))))

// Process a single 512-bit (64-byte) block
static void sha1_transform() {
    uint32_t a=sram.state[0], b=sram.state[1], c=sram.state[2];
	uint32_t d=sram.state[3], e=sram.state[4];
    uint32_t w[16]; // Only 16 words instead of 80
	uint32_t temp, f;

    // Load block and convert Big-Endian data stream to AVR native format
    for (int i = 0; i < 16; i++) {
        w[i] = ((uint32_t)sram.buffer[i * 4] << 24) |
               ((uint32_t)sram.buffer[i * 4 + 1] << 16) |
               ((uint32_t)sram.buffer[i * 4 + 2] << 8)  |
               ((uint32_t)sram.buffer[i * 4 + 3]);
    }

	// 80 rounds of processing using an on-the-fly rolling buffer
	for (uint8_t i = 0; i < 80; i++) {
		if (i >= 16) {
			// Rolling formula: update the 16-word index circulary in place
			uint8_t idx = i & 15;
			w[idx] = LEFT_ROTATE(w[(idx + 13) & 15] ^ w[(idx + 8) & 15] ^
								 w[(idx + 2) & 15]  ^ w[idx], 1);
		}
		if (i < 20) {
			f = (b & c) | (~b & d);
			temp = LEFT_ROTATE(a, 5) + f + e + 0x5A827999 + w[i & 15];
		} else if (i < 40) {
			f = b ^ c ^ d;
			temp = LEFT_ROTATE(a, 5) + f + e + 0x6ED9EBA1 + w[i & 15];
		} else if (i < 60) {
			f = (b & c) | (b & d) | (c & d);
			temp = LEFT_ROTATE(a, 5) + f + e + 0x8F1BBCDC + w[i & 15];
		} else {
			f = b ^ c ^ d;
			temp = LEFT_ROTATE(a, 5) + f + e + 0xCA62C1D6 + w[i & 15];
		}

		e = d; d = c; c = LEFT_ROTATE(b, 30); b = a; a = temp;
		
	}

    sram.state[0] += a; sram.state[1] += b; sram.state[2] += c;
	sram.state[3] += d; sram.state[4] += e;
}

static const __flash struct __attribute__((packed)) {
	char magic[36];
	uint32_t word[7];
} magic = {
	"258EAFA5-E914-47DA-95CA-C5AB0DC85B11",
	0,
    0x67452301,
    0xEFCDAB89,
    0x98BADCFE,
    0x10325476,
    0xC3D2E1F0,
	60
};

// Complete the hash and extract the final 20-byte digest
uint8_t *sha1() {
	uint32_t total_bits;
	unsigned buffer_index;

//	for (int i = 0; i < sizeof(magic); i++) sram.magic[i] = magic.magic[i];
	mem_copy(&sram.buffer[24], magic.magic, sizeof(magic));

	total_bits = sram.count * 8;
	buffer_index = (unsigned)(sram.count & 63);

	// Padding sequence
	sram.buffer[buffer_index++] = 0x80;
	if (buffer_index > 56) {
		while (buffer_index < 64) sram.buffer[buffer_index++] = 0;
		sha1_transform();
		buffer_index = 0;
	}

	while (buffer_index < 56) sram.buffer[buffer_index++] = 0;

	// Append total bit count as big-endian 64-bit integer
	for (int i = 7; i >= 0; i--) {
		sram.buffer[56 + i] = (uint8_t)(total_bits & 0xff);
		total_bits >>= 8;
	}

	sha1_transform();

	sram.count = 0;
	return (uint8_t*)sram.state;
}
