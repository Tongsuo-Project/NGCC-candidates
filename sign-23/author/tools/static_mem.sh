#!/usr/bin/env bash
#
# static_mem.sh -- NGCC static-memory metric for the SHUTTLE production
# library.
#
# Sums the ELF section sizes of the per-instance SCHEME object set:
#     static_text  = sum of .text
#     static_data  = sum of .data + .rodata
#     static_bss   = sum of .bss
#     static_total = text + data + bss
# (Eval_x86.md sec.3.1: "静态内存占用 = code + const + globals".)
#
# The measured archive contains ONLY the production scheme TUs + the XOF
# backend the scheme actually calls (drng.c+auxfunc.c under NGCC_MODE,
# fips202.c under SHA3_MODE).  It EXCLUDES KAT_SIG.c, the test/profiler/
# bench TUs, and SIG_AlgorithmInstance.c's KAT-only paths -- static memory
# must reflect the deployable library, not the harness.
#
# Implementation note: the eval-box libelf has no dev header here, so we
# read sections with `size -A` (the GNU binutils SysV section table), which
# walks the same ELF sections libelf would.  Works on both .o and .a.
#
# Usage:
#   tools/static_mem.sh [MODE]        MODE = NGCC (default) | SHA3
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
    XOF="symmetric.c fips202.c"
else
    MODEFLAG=""
    XOF="symmetric.c drng.c auxfunc.c"
fi

# The production scheme TUs (NOT KAT_SIG.c, NOT test/*, NOT the bench TUs).
SCHEME="SIG_AlgorithmInstance.c sign.c polyvec.c sampler.c sampler_u.c irs.c \
rounding.c packing.c poly.c poly_ntt.c reduce.c rans.c approx_exp.c approx_log.c"

echo "=== SHUTTLE static memory ($XMODE) -- per-instance production library ==="
printf "%-6s %12s %12s %12s %12s\n" "mode" "text" "data+rodata" "bss" "total"

for m in 128 256 512; do
    case $m in
        128) qs=q15361n256 ;;
        256) qs=q61441n512 ;;
        512) qs=q59393n1024 ;;
    esac
    archive="$OUT/libshuttle_${XMODE}_${m}.a"
    rm -f "$archive"
    objs=""
    ( cd "$REF" && \
      for src in $SCHEME $XOF ntt/$qs/ntt_ref.c; do
          o="$OUT/sm_$(echo "$src" | tr '/.' '__').o"
          $CC $CFLAGS -Intt/$qs $MODEFLAG -DSHUTTLE_MODE=$m -c "$src" -o "$o"
      done )
    objs=$(ls "$OUT"/sm_*.o)
    ar rcs "$archive" $objs
    rm -f $objs

    # Guard: KAT_SIG.c / test TUs must NOT be in the archive.
    if ar t "$archive" | grep -qiE 'KAT_SIG|test_|speed_|prof\.o|cpucycles|mem_worker'; then
        echo "  FAIL: test/KAT TU leaked into the measured archive"
        exit 1
    fi

    # Sum sections via `size -A` (SysV per-section table).
    read -r text data bss < <(
        size -A "$archive" | awk '
            $1==".text"   { t += $2 }
            $1==".data"   { d += $2 }
            $1==".rodata" { d += $2 }
            $1==".bss"    { b += $2 }
            END { printf "%d %d %d\n", t, d, b }'
    )
    total=$((text + data + bss))
    printf "%-6s %12d %12d %12d %12d\n" "$m" "$text" "$data" "$bss" "$total"
done
