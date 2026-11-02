#!/bin/sh
# gen_bulk_k.sh -- reproducible derivation of the AVX N-way gauss bulk-preset
# constants BULK_K_EXPAND_S / BULK_K_SAMPLE_Y (polyvec.h), per the
# reproducible-constants principle (no magic numbers; every code constant is
# reproducible via a generator or log).
#
# Method.  For each (sampler, mode) it builds the off-path harness
# ref/test/measure_jk.c against the SCALAR ref sampler (which draws exactly
# one XOF block per gs_fill, so the MEASURE_JK hook counts each lane's block
# count J), runs N calls x XOF_STREAMS lanes, and pins
#
#     K* = smallest K with  P(J > K) <= 1/16
#
# the zero-crossing of the bulk cost model E[cost](K) = K + 16*sum_{j>K}
# (j-K) P(J=j): one extra pre-filled block costs one N-way squeeze and saves
# the ~1-of-16 lanes still active past K, so it pays for itself exactly while
# P(J>K) > 1/16.  The fallback path is mandatory regardless (a fixed K can be
# exceeded by an unlucky seed), so K only trades bulk-waste against
# fallback-cost -- it is never a correctness constant (KAT is identical for
# any K).  J is governed by the public lane width + rejection rate, identical
# in distribution across NGCC/SHA3, so one NGCC measurement pins K for both.
#
# Output: prints the BULK_K_* macro block to stdout and writes the full
# histograms + K* derivation to tools/gen_bulk_k/bulk_k.log.  With --check it
# also diffs the derived values against the macros currently in polyvec.h and
# exits nonzero on any mismatch (use in CI to keep the constants honest).
set -eu
cd "$(dirname "$0")/.."   # SHUTTLE/
REF=ref
OUTDIR=tools/gen_bulk_k
LOG="$OUTDIR/bulk_k.log"
CALLS=${BULK_K_CALLS:-20000}
CC=${CC:-gcc}
CHECK=0
[ "${1:-}" = "--check" ] && CHECK=1

mkdir -p "$OUTDIR" "$REF/out"

# Scalar-ref source list sufficient for expand_s / sample_y (no NTT): mirrors
# the Makefile's test_samplers list + the NGCC xof backend.
SRCS="polyvec.c sampler.c reduce.c approx_exp.c approx_log.c symmetric.c drng.c auxfunc.c"
CFLAGS="-std=c99 -O2 -I. -Itest -I../tools -DMEASURE_JK"

{
    printf '=== SHUTTLE bulk-preset K* derivation ===\n'
    printf 'date_utc: '; date -u '+%Y-%m-%dT%H:%M:%SZ'
    printf 'criterion: K* = smallest K with P(J>K) <= 1/16\n'
    printf 'calls/cell: %s   lanes: XOF_STREAMS=16   samples/cell: %s\n\n' \
        "$CALLS" "$((CALLS * 16))"
} > "$LOG"

# kstar <histfile> -> prints "Kstar Pgt" (P(J>Kstar-1) is >1/16, P(J>Kstar)<=).
kstar() {
    awk '
        /^#/ { next }
        { c[$1]=$2; if ($1+0 > jmax) jmax=$1+0; tot+=$2 }
        END {
            for (K = 0; K <= jmax; K++) {
                s = 0
                for (j = K + 1; j <= jmax; j++) s += c[j]
                if (s * 16 <= tot) {
                    printf "%d %.4f\n", K, s / tot
                    exit
                }
            }
            printf "%d %.4f\n", jmax, 0.0
        }' "$1"
}

emit_macro() {  # mode  K_expand_s  K_sample_y
    if [ "$1" = 128 ]; then printf '#if SHUTTLE_MODE == 128\n'
    elif [ "$1" = 256 ]; then printf '#elif SHUTTLE_MODE == 256\n'
    else printf '#elif SHUTTLE_MODE == 512\n'; fi
    printf '#    define BULK_K_EXPAND_S %s\n' "$2"
    printf '#    define BULK_K_SAMPLE_Y %s\n' "$3"
}

rc=0
MACRO_BLOCK=""
for mode in 128 256 512; do
    for s in 0 1; do
        bin="$REF/out/measure_jk_${mode}_${s}"
        # shellcheck disable=SC2086
        ( cd "$REF" && $CC $CFLAGS -DSHUTTLE_MODE=$mode -DMEASURE_SAMPLER=$s \
            test/measure_jk.c $SRCS -lm -o "out/measure_jk_${mode}_${s}" )
        hist="$OUTDIR/hist_${mode}_${s}.txt"
        "$bin" "$CALLS" > "$hist"
        set -- $(kstar "$hist")
        K=$1; Pgt=$2
        name=$( [ "$s" = 0 ] && echo expand_s || echo sample_y )
        {
            printf '## sampler=%s mode=%s\n' "$name" "$mode"
            head -1 "$hist"
            printf 'K* = %s   (P(J>K*) = %s <= 1/16)\n' "$K" "$Pgt"
            tail -n +2 "$hist" | awk '{printf "  J=%s  n=%s\n",$1,$2}'
            printf '\n'
        } >> "$LOG"
        eval "K_${mode}_${s}=$K"
    done
    es=$(eval echo \$K_${mode}_0); sy=$(eval echo \$K_${mode}_1)
    MACRO_BLOCK="$MACRO_BLOCK$(emit_macro "$mode" "$es" "$sy")
"
done
printf '#endif\n' >> "$LOG"

printf '%s#endif\n' "$MACRO_BLOCK"
printf '\n(full derivation logged to %s)\n' "$LOG" >&2

if [ "$CHECK" = 1 ]; then
    # Compare each derived macro against the current polyvec.h value.
    for mode in 128 256 512; do
        for pair in "EXPAND_S 0" "SAMPLE_Y 1"; do
            set -- $pair; tag=$1; s=$2
            want=$(eval echo \$K_${mode}_${s})
            have=$(awk -v m="$mode" '
                $0 ~ "SHUTTLE_MODE == " m {inblk=1}
                inblk && $2 == "define" && $3 == "BULK_K_'"$tag"'" {print $4; exit}
            ' "$REF/polyvec.h")
            if [ "$want" != "$have" ]; then
                printf 'MISMATCH BULK_K_%s mode=%s derived=%s polyvec.h=%s\n' \
                    "$tag" "$mode" "$want" "$have" >&2
                rc=1
            fi
        done
    done
    [ "$rc" = 0 ] && printf 'CHECK: polyvec.h BULK_K_* match derived values\n' >&2
fi
exit $rc
