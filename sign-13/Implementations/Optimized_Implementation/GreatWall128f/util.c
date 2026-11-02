#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200112L
#endif

#include "util.h"
#include <stdio.h>
#include <stdlib.h>

void* greatwall_aligned_alloc(size_t alignment, size_t size)
{
    void* ptr = NULL;
    if (alignment == 0 || (alignment & (alignment - 1)) != 0)
        return NULL;
    if (alignment < sizeof(void*))
        alignment = sizeof(void*);
    if (posix_memalign(&ptr, alignment, size) != 0)
        return NULL;
    return ptr;
}

extern inline unsigned int count_trailing_zeros(uint64_t x);
extern inline uint8_t expand_bit_to_byte(unsigned long x, unsigned int i);
extern inline void expand_bits_to_bytes(uint8_t* output, size_t num_bits, size_t x);

void printHex(const char* s, const uint8_t* data, size_t len) {
  printf("%s: ", s);
  for (size_t i = 0; i < len; i++) {
    printf("%02X", data[i]);
  }
  printf("\n");
}
