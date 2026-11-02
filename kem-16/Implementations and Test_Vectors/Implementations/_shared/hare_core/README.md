# HARE shared core

```text
common/        PKE/KEM, code layer, parsing, parameter contracts, generated RS constants
ref/           portable C99 vector/GF/GF2X/RM/RS/compression kernels
x86_64/        AVX2/PCLMUL optimized kernels
aarch64/       ARM/SVE additional kernels
```

All platform kernels preserve the same public API and algorithm semantics as the
Reference path.  Optimized GF2X implementations remain dense and public-size;
they do not use secret sparse support multiplication.
