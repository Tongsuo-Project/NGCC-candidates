# broadwell GF backend generators (saturated-limb, mulx/adcx/adox)

Tooling that produced `src/gf/broadwell/lvlN/` for new primes p = c·2^f − 1.
lvl2 (c=9,f=309,N=5,tag=gf9309) is generated, GMP-validated, passes project gf
tests AND the full scheme test. The generated code is STRUCTURALLY ON-PAR with
the committed lvl1/3/5 (asm fp_add/sub/mul/sqr + asm fp2_mul_c0/c1/sq_c0/c1 +
safegcd inversion/Legendre).

## Files
- `levels.py`, `magics.py` : per-level constants + reduction magics (validated byte-exact vs committed lvl1/3/5).
- `gen_asm.py N c f`        : emit fp_asm.S — fp_add/sub/mul/sqr AND fp2_mul_c0/c1, fp2_sq_c0/c1. Byte-reproduces committed lvl5 core (N=8). Register-only Montgomery: valid for N<=8 only (lvl6/N=16 needs a stack-accumulator redesign).
- `gen_gf.py name c f N tag`: emit gf<tag>.{h,c} + fp.c. gf_mul/gf_square delegate to asm fp_mul/fp_sqr; sqrt + fp_exp3div4 via gf<tag>_pow (square-and-multiply); invert/legendre emitted as Fermat placeholders (replaced by safegcd, below).
- `gen_safegcd.py N c f tag`: emit the safegcd C (gf_lin, lindiv31abs, gf_div, gf_invert, gf_legendre + INVT). Ported from lvl1/3/5; iteration counts outer=4N-1, inner=31, final=2*nbits-2-31*outer.
- `splice.py`               : remove the Fermat invert/legendre + EXP_INV/EXP_LEG from gen_gf.py's gf<tag>.c and splice in the safegcd fragment.
- `driver.c`, `driver_fp2.c`, `edge.c` : GMP cross-check harnesses (link gf<tag>.c + fp_asm.S, -DDISABLE_NAMESPACING, -lgmp).

## Regenerate lvl2 (example)
    python3 gen_asm.py 5 9 309            > lvl2/fp_asm.S
    python3 gen_gf.py  lvl2 9 309 5 gf9309    # writes out_lvl2/{gf9309.h,gf9309.c,fp.c}
    python3 gen_safegcd.py 5 9 309 gf9309 > safegcd_lvl2.c
    python3 splice.py                          # merges safegcd into out_lvl2/gf9309.c
Then drop into src/gf/broadwell/lvl2/ (fp2.h = NO_FP2X_MUL/SQR + asm wrappers).

## Build a broadwell test (out-of-tree)
    cmake -G Ninja -DSQISIGN_BUILD_TYPE=broadwell -DCMAKE_BUILD_TYPE=Release \
          -DGMP_LIBRARY=SYSTEM -DGMP=<gmp>/lib/libgmp.so -DGMP_INCLUDE=<gmp>/include <repo>
    ninja sqisign_test_gf_lvl2_fp sqisign_test_gf_lvl2_fp2
NOTE: a broadwell build requires SVARIANT_S to contain ONLY levels that have
src/gf/broadwell/lvlN (currently lvl1;lvl2;lvl3;lvl5). lvl6 broadwell not yet done.
