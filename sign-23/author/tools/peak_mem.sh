#!/usr/bin/env bash
#
# peak_mem.sh -- NGCC peak-memory metric for SHUTTLE.
#
# Runs the one-shot keygen+sign+verify worker (ref/test/mem_worker.c) under
#   valgrind --tool=massif --stacks=yes
# and reports peak = max(mem_heap_B) + max(mem_stacks_B) over the run
# (Eval_x86.md sec.3.1: "峰值内存占用 = heap + stack peak, measured at
# runtime").  Also prints pk/sk/sig sizes (the worker emits them).
#
# Fallback: if valgrind/massif is unavailable, the worker self-reports its
# peak RSS via getrusage(RUSAGE_SELF).ru_maxrss; the report then labels
#   peak_source=maxrss
# (comparability across schemes degrades -- flag in the self-eval report).
#
# Usage:
#   tools/peak_mem.sh [MODE]          MODE = NGCC (default) | SHA3
#   (loops modes 128/256/512)
#
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
REF="$HERE/../ref"
OUT="$REF/out"
mkdir -p "$OUT"

XMODE="${1:-NGCC}"
CC="${CC:-gcc}"
CFLAGS="-std=c99 -Wpedantic -Wall -Wextra -O2 -I. -Itest -I../tools"

if [ "$XMODE" = "SHA3" ]; then
    MODEFLAG="-DSHA3_MODE"
    XOF="symmetric.c fips202.c drng.c auxfunc.c"
else
    MODEFLAG=""
    XOF="symmetric.c drng.c auxfunc.c"
fi

SCHEME="SIG_AlgorithmInstance.c sign.c polyvec.c sampler.c sampler_u.c irs.c \
rounding.c packing.c poly.c poly_ntt.c reduce.c rans.c approx_exp.c approx_log.c"

have_valgrind=0
command -v valgrind >/dev/null 2>&1 && have_valgrind=1

echo "=== SHUTTLE peak memory ($XMODE) -- one keygen+sign+verify under massif ==="
printf "%-6s %14s %14s %14s %12s %s\n" \
    "mode" "peak_heap_B" "peak_stack_B" "peak_total_B" "maxrss_KiB" "source"

for m in 128 256 512; do
    case $m in
        128) qs=q15361n256 ;;
        256) qs=q61441n512 ;;
        512) qs=q59393n1024 ;;
    esac
    bin="$OUT/mem_worker_${XMODE}_${m}"
    ( cd "$REF" && \
      $CC $CFLAGS -Intt/$qs $MODEFLAG -DMEM_WORKER_MAIN -DSHUTTLE_MODE=$m \
          test/mem_worker.c $SCHEME $XOF ntt/$qs/ntt_ref.c -o "$bin" )

    mo="$OUT/massif_${XMODE}_${m}.out"
    rm -f "$mo"
    # Always run the worker once to capture maxrss (and confirm it works).
    rss=$("$bin" | sed -n 's/^maxrss_kib=//p')

    heap=0; stack=0; source="maxrss"
    if [ "$have_valgrind" = "1" ]; then
        if valgrind --tool=massif --stacks=yes --massif-out-file="$mo" \
                    --quiet "$bin" >/dev/null 2>&1 && [ -f "$mo" ]; then
            heap=$(awk -F= '/^mem_heap_B=/   { if ($2>m) m=$2 } END { print m+0 }' "$mo")
            stack=$(awk -F= '/^mem_stacks_B=/ { if ($2>m) m=$2 } END { print m+0 }' "$mo")
            source="massif"
        fi
    fi

    total=$((heap + stack))
    printf "%-6s %14d %14d %14d %12s %s\n" \
        "$m" "$heap" "$stack" "$total" "${rss:-0}" "peak_source=$source"
done
