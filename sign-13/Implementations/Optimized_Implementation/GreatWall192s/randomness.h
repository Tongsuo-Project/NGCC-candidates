#ifndef RANDOMNESS_H
#define RANDOMNESS_H

#include <stddef.h>
#include <stdint.h>

#define GREATWALL_USE_OS_RANDOM 1

int rand_bytes(uint8_t* dst, size_t num_bytes);

#endif
