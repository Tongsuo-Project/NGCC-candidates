#ifndef GREATWALL_KEYGEN_GF197_H
#define GREATWALL_KEYGEN_GF197_H

#include <stdbool.h>
#include <stdint.h>

#ifndef GREATWALL_KEYGEN_GF197_ISOLATED
#define GREATWALL_KEYGEN_GF197_ISOLATED 1
#endif

bool greatwall_keygen_pubkey_197(uint8_t* pk, const uint8_t* sk);

#endif
