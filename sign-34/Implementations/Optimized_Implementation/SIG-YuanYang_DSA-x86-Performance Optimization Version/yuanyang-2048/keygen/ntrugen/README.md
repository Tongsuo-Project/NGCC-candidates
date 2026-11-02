# ntrugen

This directory vendors the standalone ntrugen solver used by YuanYang DSA key generation.

## Layout

- `src/ntrugen.c` and `src/ntrugen.h`: direct solver interface.
- `src/bigint*`, `src/cfft*`, `src/complex*`, and `src/fft*`: arithmetic support.
- `src/twiddles*.c`: parameter-set-specific twiddle tables.

## Interface

`ntrugen(f, g, F, G, logn, q, work)` returns:

- `NTRUGEN_OK` on success;
- `NTRUGEN_ERR_GCD` when the resultant gcd step fails;
- `NTRUGEN_ERR_VERIFY` when the reconstructed basis does not pass the solver check.

Key generation samples `f` and `g`, computes the public key, calls `ntrugen()`, converts the returned Bezout pair into the YuanYang basis convention, and restarts the keygen attempt if the solver fails or if the converted basis does not fit the compact secret-key encoding.
