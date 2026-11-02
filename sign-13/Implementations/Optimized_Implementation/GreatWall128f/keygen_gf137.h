#ifndef GREATWALL_KEYGEN_GF137_H
#define GREATWALL_KEYGEN_GF137_H

#include <stdbool.h>
#include <stdint.h>

#ifndef GREATWALL_KEYGEN_GF137_ISOLATED
#define GREATWALL_KEYGEN_GF137_ISOLATED 1
#endif
#if GREATWALL_KEYGEN_GF137_ISOLATED != 0 && GREATWALL_KEYGEN_GF137_ISOLATED != 1
#error "GREATWALL_KEYGEN_GF137_ISOLATED must be 0 or 1"
#endif

bool greatwall_keygen_pubkey_137(uint8_t* pk, const uint8_t* sk);

#endif
