# TCHES 2021 NTT experimental backend import

This directory contains the minimal AVX2 Saber NTT multiplication components imported
from the public artifact for:

> Chi-Ming Marvin Chung, Vincent Hwang, Matthias J. Kannwischer, Gregor Seiler,
> Cheng-Jhih Shih, and Bo-Yin Yang, "NTT Multiplication for NTT-unfriendly Rings:
> New Speed Records for Saber and NTRU on Cortex-M4 and AVX2," TCHES 2021.

Upstream artifact: https://github.com/ntt-polymul/ntt-polymul

Imported files are copied from the upstream `avx2/` implementation and are kept under
upstream CC0/public-domain licensing; see `LICENSE.ntt-polymul` and
`README.upstream.md`.  Viper only enables this code when the opt-in build macro
`VIPER_EXPERIMENTAL_TCHES2021_NTT=1` is set.  The default Viper arithmetic backend is
unchanged.

For Viper, the imported Saber AVX2 NTT code is compiled with the corresponding Saber
parameter macro selected from `VIPER_LEVEL` (`LIGHTSABER`, `SABER`, or `FIRESABER`).
The upstream Saber NTT reconstructs products modulo Saber q=8192 using two 16-bit NTT
prime moduli, 7681 and 10753, and CRT.  The Viper wrapper masks the result to Viper
q=4096 and keeps all conversions inside the arithmetic layer.
