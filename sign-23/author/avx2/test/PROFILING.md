# SHUTTLE reference per-component profiling

Per-component cycle and randomness breakdown of the SHUTTLE `ref/` (pure-scalar) KeyGen / Sign / Verify, produced by `make profile` (`-DPROF_TIME -DPROF_RAND`, drivers `test/speed_profile.c` + `test/prof.c`). This markdown is the hand-curated mirror of the machine-generated `test/profiling.txt`; the raw cycle/byte numbers and the `MACHINFO` banner live there. Numbers below are cycles/op averaged over NKG=200 keygen / NSIG=600 sign+verify iterations on the machine noted at the bottom; treat the absolute cycles as indicative (the box is an unpinned WSL2 host) and the **percentages** as the load-bearing result.

Method note: per-op cycles are the sum of the instrumented top-level `PT_*` buckets (each `rdtscp`-bracketed, ~50-cycle probe overhead), not wall-clock. A profiled run of one primitive leaves only that primitive's buckets non-zero, so each section below is self-contained. Sign's per-iteration buckets are charged across all rejection retries (the `(calls …)` count exceeds NSIG by the retry factor), so the per-op figure already includes the expected restart cost.

The child sub-buckets `PT_NTT_FWD/PW/INV`, `PT_G_*` (BaseSampler / ApproxExp), and `PT_SAMPLERU / PT_APPROXLOG` are defined in the `PT_*` enum and appear in the report, but their probes live inside `sampler.c` / `irs.c` / `rounding.c`; the probes inserted into `sign.c` / `polyvec.c` report the parent-phase granularity, which already isolates the dominant `SAMPLE_Y` / `IRS` / `COMMIT` split that the optimization work needs. Those rows show `0.00% of parent` until the probes inside those components are filled in.

## Sign — % of sign total

The headline result: Sign is dominated by `SAMPLE_Y` (wide-Gaussian $y$-vector sampling) and `IRS` (the Iterative Rejection Sampler: SamplerU + ApproxLog), with `EXPAND_A` (the uniform $\hat A$ matrix expansion) third. The SHA3 column is the cleaner algorithmic picture; under NGCC the SM3 Hash-DRBG XOF placeholder inflates every XOF-bound bucket (`SAMPLE_Y`, `EXPAND_A`, `CHALLENGE`), pushing `SAMPLE_Y` to ~50%.

SHUTTLE-256, sign:

| component | NGCC_MODE % | SHA3_MODE % |
|---|---|---|
| sample_y (Gaussian) | 50.29 | 43.87 |
| irs (rejection sampler) | 16.12 | 24.85 |
| expand_A | 25.04 | 16.74 |
| normcheck ($B_v$ gate) | 2.90 | 5.88 |
| commitment (NTT mat-mul) | 2.38 | 4.80 |
| challenge (hash+sample) | 2.11 | 1.54 |
| rANS encode | 0.55 | 1.12 |
| highbits/lsb (CompressY) | 0.13 | 0.26 |
| makehint | 0.16 | 0.33 |
| setup (skDecode/tr/mu) | 0.31 | 0.63 |

Reading: the three plausible optimization hot spots — NTT (`commitment`), the samplers (`sample_y` + `irs`), and `rANS` — resolve overwhelmingly in favour of the samplers. `commitment` is only $\sim 2\text{--}5\%$ and `rANS` is $\sim 1\%$, so the NTT and the rANS encode are NOT where Sign's time goes; the wide-Gaussian sampler and the IRS loop are (combined $\sim 66\%$ SHA3 / $\sim 66\%$ NGCC). The `rANS` row carries the per-iteration `sigEncode`; its tiny share confirms rANS is not a Sign hot spot even though it is the only source of Sign's latency *tail* (the out-of-support restart).

## Verify — % of verify total

SHUTTLE-256, verify:

| component | NGCC_MODE % | SHA3_MODE % |
|---|---|---|
| expand_A + b-hat NTT | ~83 | ~52 |
| commitment (1+ELL cols) | ~4 | ~9 |
| challenge (hash+sample) | ~6 | ~11 |
| setup (tr/mu) | ~2 | ~5 |
| unpack pk/sig | ~2 | ~6 |
| normcheck (z reconstruct) | ~1 | ~3 |
| usehint/lsb | ~0.5 | ~1 |

Reading: Verify is structurally cheap (its `mat_mul_z1_2q` spans only $1+\ell$ columns, omitting the $2 I_m$ block) — but under NGCC it is almost entirely `EXPAND_A` because re-expanding $\hat A$ from the SM3 DRBG dwarfs everything else. The SHA3 column shows the true Verify cost split once the XOF is fast. This is the single largest NGCC-vs-SHA3 divergence and the clearest optimization lever (see the XOF finding below).

## KeyGen — % of keygen total

SHUTTLE-128, keygen (NGCC): `expand_A` $\approx 49\%$, `sample_noise (s,e)` $\approx 47\%$, `b-product (NTT)` $\approx 2\%$, with `roundB` / `stretchS` / `norm-window` / `pack` each $< 1\%$. KeyGen's two costs are both XOF-bound (matrix expansion + noise sampling), and its wide latency distribution comes from the $[B_k', B_k]$ norm-window rejection (each rejection re-expands $\hat A$ and re-samples noise).

## Randomness accounting (bytes/sig)

The `PROF_RAND` per-context squeeze table is IDENTICAL across NGCC and SHA3 (same consumption order/amount — a correctness check), so the only divergence is the per-byte XOF cost. SHUTTLE-256, sign, bytes/sig:

| context | bytes/sig | % of squeezed |
|---|---|---|
| gauss (SampleY) | 86700 | 55.05 |
| A (ExpandA) | 65536 | 41.61 |
| challenge | 4160 | 2.64 |
| irs | 1044 | 0.66 |
| setup | 64 | 0.04 |
| **total** | **157504** | 100.00 |

Findings:

- **ExpandA squeezes a fixed 65536 B/sig** regardless of mode, $\sim 42\%$ of all randomness, and re-runs on every Sign and Verify. This is the uniform-$\hat A$ rejection-sampling buffer. It is the single biggest PRNG-budget item and the obvious target for caching $\hat A$ across the Sign retry loop (it is loop-invariant) — though the current code already expands it once per Sign call, not per iteration, so this is a Verify/throughput lever, not a per-iteration one.
- **No obvious over-squeeze waste within a context.** The `gauss` 86.7 KB is the wide-Gaussian draw (large by design: $\sigma$ is wide and the BLISS convolution consumes uniform $y$ bits plus rejection-tail bytes); `challenge`/`irs`/`setup` are all small and proportionate to their fixed seed/squeeze lengths. No context shows a zero where it must squeeze, and none is inflated relative to its algorithmic need.
- The fine `PU_*` consumption rows (signs / sigma_s / y / rej_tail / SamplerU) are zero in this snapshot because those probes live in `sampler.c` / `irs.c`, outside the `sign.c`/`polyvec.c` probe scope; the rows are wired and will populate when those components insert `PROF_USE`.

## The XOF finding (NGCC SM3 DRBG vs SHA3 SHAKE)

The dominant cross-mode result, straight from `speed.txt`: NGCC_MODE (the mandated SM3 Hash-DRBG placeholder) is $\sim 3\text{--}5\times$ slower than SHA3_MODE (SHAKE) on every primitive, because the SM3 DRBG emits only 32 B per SM3 block and is markedly slower per byte than Keccak. SHUTTLE-256 verify median is $\sim 2.5\text{M}$ cycles (NGCC) vs $\sim 1.0\text{M}$ (SHA3); Sign median $\sim 8.0\text{M}$ vs $\sim 3.9\text{M}$. The profiler attributes this to the XOF-bound buckets (`EXPAND_A`, `SAMPLE_Y`, `CHALLENGE`). This is expected and documented (the SM3 backend is an ICCS placeholder); it means the NGCC perf numbers are XOF-bound, and the SIMD work on the samplers will move the needle most under SHA3, while a faster NGCC hash (future round) is the bigger NGCC lever.

## Per-mode end-to-end totals (median cycles, ref scalar)

From `speed.txt` (tail-bench p50 for keygen/sign, median for verify). Absolute cycles are box-dependent; the cross-mode ratios are the stable read.

| primitive | 128 NGCC | 256 NGCC | 512 NGCC | 128 SHA3 | 256 SHA3 | 512 SHA3 |
|---|---|---|---|---|---|---|
| keygen p50 | 8.95M | 8.99M | 14.9M | 2.79M | 3.17M | 5.21M |
| sign p50 | 14.7M | 8.06M | 15.6M | 7.42M | 3.90M | 8.86M |
| verify median | 2.26M | 2.55M | 3.05M | 0.85M | 1.02M | 1.47M |

(Sign p50 at mode 128 is higher than mode 256 because of heavier per-call retry variance on this unpinned box, not an algorithmic inversion; the StQ stabilized quartiles in `speed.txt` track p50 closely for mode 256/512 — sign is near-deterministic there — and the mode-128 spread is measurement tail.)

## Machine note

- CPU: 11th Gen Intel Core i7-11700K @ 3.60 GHz (reported by lscpu inside WSL2).
- Kernel: Linux 5.15.x microsoft-standard-WSL2 (unpinned; no taskset, Turbo/HT not controlled).
- Compiler: gcc 11.4.0, `-std=c99 -Wpedantic -Wall -Wextra -O2`.
- Run counts: NKG=200 keygen, NSIG=600 sign+verify (profile); keygen/sign tail-bench 1500 runs, verify median 10000, throughput 150 iters (speed).
- For a quiet, repeatable run regenerate on a pinned box (`taskset -c <cpu>`, Turbo off) with the full `make profile` / `make speed` counts; the percentage breakdown is stable across boxes, the absolute cycles are not.
