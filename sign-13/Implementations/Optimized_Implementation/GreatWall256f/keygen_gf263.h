#ifndef GREATWALL_KEYGEN_GF263_H
#define GREATWALL_KEYGEN_GF263_H

#include <stdbool.h>
#include <stdint.h>

#ifndef GREATWALL_KEYGEN_GF263_ISOLATED
#define GREATWALL_KEYGEN_GF263_ISOLATED 1
#endif

bool greatwall_keygen_pubkey_263(uint8_t* pk, const uint8_t* sk);

#endif
