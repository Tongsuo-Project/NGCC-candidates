# QCTM512 compressed GammaPrime implementation notes

This directory is a new implementation variant derived from the eta-from-bits
QCTM512 code path. It follows the same compressed GammaPrime contract used by
the QCTM128/QCTM256 compressed variants.

## Serialization contract

- `GammaPrime = g || L_prime`
- `skKEM = GammaPrime || pk_copy || s`
- `pk = psi_T1 || T2`
- `ct = C`
- `ss = 64 bytes`

The API secret key includes `pk_copy` because the official `kem_dec(sk, ct)`
entry point does not pass `pk`. The `pk_copy` is not part of the mathematical
private decoding structure.

The secret key must not serialize transient decoding data:

- no `decode_map`
- no `Hhat_T`
- no expanded public matrix `H`
- no transform matrix `D`
- no row-operation matrix `Q`
- no column permutation `tau`
- no original support `L`

## Fixed QCTM512 lengths

- `PK_BYTES = 2374727`
- `GAMMA_BYTES = 87766`
- `S_BYTES = 4750`
- `SECRETKEY_BYTES = 2467243`
- `CT_BYTES = 2264`
- `SS_BYTES = 64`

These are enforced by compile-time assertions in `sizes.h`.

## Verified status

Verified locally:

- `make -B roundtrip`
- `make verify-decode-map`
- `make -B api-pkc-kat`

The official API/KAT executable was started after these checks. If the KAT file
is regenerated, verify:

- all 10 counts are present
- `PK_Len = 2374727`
- `SK_Len = 2467243`
- `CT_Len = 2264`
- `SS_Len = 64`
- SHA256:
  `dd9c996b0e0a7b6d5179335c9663d325247b658131ee3cb13adb4c5638315002`
