# GreatWall160f

This directory is self-contained: source files, matrix constants and build rules
are local. No shared GreatWall160 directory or sibling profile is required.

- Native OWF tuple: (160, 163, 79, 86).
- Native field: GF(2^163), modulo X^163 + X^7 + X^6 + X^3 + 1.
- Authentication field: GF(2^160), modulo X^160 + X^5 + X^3 + X^2 + 1.
- (tau, w, T_open): (20, 0, 160).
- Public and secret keys: 42 bytes each.
- Signature: 5720 bytes, excluding the message.

Run on Linux/WSL with GCC, GNU Make, OpenSSL development headers and a CPU
supporting AVX2, AES-NI and PCLMULQDQ:

```bash
make -j8
./KAT_SIG
make clean
make -j8 HASH_BACKEND=sm3
./KAT_SIG
make clean
```

The default backend is SM3. Backend/flag changes automatically rebuild objects.
Use HASH_BACKEND=shake explicitly for non-submission SHAKE experiments.
The KAT executable writes into output/ and links randomness_kat_fixed.c instead
of the OS random source. Each case resets the SM3 DRNG using its recorded Seed.
Build with HASH_BACKEND=sm3 to reproduce the archived fixed SM3 vectors.
The production library retains randomness_randombytes.c and OS randomness;
the fixed KAT adapter is never included in libgreatwall160.a.

Tree/VOLE PRGs use AES-192-CTR with seed[20] followed by four zero bytes.
Serialized seeds and MACs remain 20 bytes. These profiles remain experimental,
not standardized or security-certified. The directory split changes no
arithmetic, constants, protocol parameters or optimization choices.
