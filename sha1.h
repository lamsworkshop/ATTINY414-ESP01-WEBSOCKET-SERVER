#ifndef SHA1_H
#define SHA1_H

#include <stdint.h>

uint8_t *sha1();
void mem_copy(char *dest, const __flash char *src, uint8_t size);

#endif

