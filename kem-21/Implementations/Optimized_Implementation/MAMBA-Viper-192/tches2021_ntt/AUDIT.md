# TCHES 2021 NTT artifact audit for Viper experiment

Artifact: `ntt-polymul/ntt-polymul`, retrieved from
https://github.com/ntt-polymul/ntt-polymul during this experiment.  The copied files are
from upstream `avx2/` and are licensed under the upstream CC0/public-domain license kept
as `LICENSE.ntt-polymul`.

## Upstream paths reviewed

* Saber AVX2 NTT polynomial/vector multiplication: `avx2/poly.c`, `avx2/polyvec.c`,
  `avx2/poly.h`, `avx2/polyvec.h`, `avx2/ntt256n.S`, `avx2/invntt256n.S`,
  `avx2/basemul256x1.S`, `avx2/consts256n7681.c`, `avx2/consts256n10753.c`, and
  `avx2/consts256.h`.
* Upstream Saber AVX2 Toom-Cook comparison code: `avx2/sabermul/`.
* Reference/Cortex-M4 routes also exist in the artifact under `m4/`; those were audited
  for context but not imported into Viper because this round implements only the AVX2
  experimental backend.

## Arithmetic properties

* Ring convention: Saber AVX2 path is negacyclic with `KEM_N=256`, i.e.
  `Z_q[X]/(X^256+1)`.
* Upstream Saber q: `KEM_Q=8192`.  Viper q remains `4096`; the wrapper converts Viper
  coefficients to centered `[-2048,2047]`, calls the upstream exact Saber NTT product,
  and masks output into canonical `[0,4096)`.
* NTT moduli: the length-256 negacyclic path uses the two 16-bit NTT primes `7681` and
  `10753`, followed by CRT reconstruction.
* CRT product: `7681 * 10753 = 82593793` (`82593793` exact product is not needed by the
  code; the bound below only needs the conservative product value greater than 82M).
* Matrix-vector batching: upstream `polyvec_matrix_vector_mul` transforms the secret once
  per CRT modulus and performs row-wise vector multiply/accumulate in NTT domain;
  `polyvec_iprod` does vector inner products in NTT domain.
* Build flags: Viper compiles the import only under `VIPER_EXPERIMENTAL_TCHES2021_NTT=1`
  with AVX2 flags and the matching Saber macro selected from the Viper level
  (`LIGHTSABER`, `SABER`, or `FIRESABER`).

## Coefficient-growth bound for Viper use

For the supported Viper products, dense coefficients are centered in `[-2048,2047]` and
eta=2 short coefficients are in `[-2,2]`.  One negacyclic product coefficient is the
signed sum/difference of 256 dense-by-short terms, so
`|c_i| <= 256 * 2048 * 2 = 1,048,576`.  A dot product over `k <= 4` such products has
`|c_i| <= 4 * 1,048,576 = 4,194,304`.  This is far below half of the CRT product
`7681 * 10753 / 2 = 41296896`, so reconstruction is exact before reducing modulo Viper
q=4096.  The eta=3 validation path has
`4 * 256 * 2048 * 3 = 6,291,456`, also below the CRT half range.

Dense-by-dense products can exceed this CRT half-range (`256 * 2048^2 = 1,073,741,824`),
so the Viper wrapper only claims exact support for dense-by-short, short-by-dense, and
Viper dot/matrix-vector products.  Dense-dense is covered only by the schoolbook oracle in
validation and is not claimed as a backend-supported mode.
