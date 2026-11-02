#ifndef GREATWALL_KEYGEN_GF163_H
#define GREATWALL_KEYGEN_GF163_H

#include <stdbool.h>
#include <stdint.h>

#ifndef GREATWALL_KEYGEN_GF163_ISOLATED
#define GREATWALL_KEYGEN_GF163_ISOLATED 1
#endif

bool greatwall_keygen_pubkey_163(uint8_t* pk, const uint8_t* sk);

#endif
