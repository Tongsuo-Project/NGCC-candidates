#ifndef GREATWALL_KEYGEN_GF137_H
#define GREATWALL_KEYGEN_GF137_H

#include <stdbool.h>
#include <stdint.h>

#ifndef GREATWALL_KEYGEN_GF137_ISOLATED
#define GREATWALL_KEYGEN_GF137_ISOLATED 1
#endif

bool greatwall_keygen_pubkey_137(uint8_t* pk, const uint8_t* sk);

#endif
