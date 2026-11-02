#!/bin/sh
# SHUTTLE signing variable-latency (KyberSlash/TIMECOP) smoke for one backend +
# parameter mode. Marks the signing-secret region of sk + the signing
# randomness undefined (tools/dudect/timecop_smoke.c) and runs one
# keygen+sign+verify under the patched variable-latency Memcheck. GATE = the
# KyberSlash property: FAIL iff any secret operand reaches a variable-latency
# instruction. Secret-dependent control flow is out of scope (owned by ct_scan +
# dudect-sign); the branchless-CT cmov idioms (Memcheck:Cond) are suppressed via
# shuttle-ct.supp.
#
#   ref     scalar reference  -- runnable under Valgrind.
#   avx2    AVX2 SIMD (-mno-avx512f) -- runnable.
#   avx512  AVX-512 SIMD -- Valgrind 3.23 SIGILLs on EVEX/VBMI2 -> SKIP (exit 3);
#           variable-latency covered statically by ct_scan + dudect-sign.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
. "$ROOT/tools/audit_lib.sh"
SUPP=${TIMECOP_SUPP:-$ROOT/tools/timecop/shuttle-ct.supp}
HARNESS=$ROOT/tools/dudect/timecop_smoke.c
VG=
BACKEND=ref
MODE=128
XOF=${TIMECOP_XOF:-ngcc}
while [ $# -gt 0 ]; do
  case "$1" in
    --valgrind) VG=$2; shift 2;;
    --backend) BACKEND=$2; shift 2;;
    --mode) MODE=$2; shift 2;;
    *) echo "usage: $0 --valgrind PATH [--backend ref|avx2|avx512] [--mode 128|256|512]" >&2; exit 2;;
  esac
done
[ -n "$VG" ] || { echo "usage: $0 --valgrind PATH [--backend ...] [--mode ...]" >&2; exit 2; }
case "$VG" in /*) ;; *) VG=$(CDPATH= cd -- "$(dirname "$VG")" && pwd)/$(basename "$VG");; esac
VG_INCLUDE=${VALGRIND_INCLUDE:-$(CDPATH= cd -- "$(dirname "$VG")/../include" 2>/dev/null && pwd || true)}
VG_INC_FLAG=
[ -n "$VG_INCLUDE" ] && [ -d "$VG_INCLUDE/valgrind" ] && VG_INC_FLAG="-I$VG_INCLUDE"

cc=${CC:-gcc}
cflags=$(audit_cflags "$BACKEND" "$MODE" "$XOF")
# add debug/frame-pointer for clean Valgrind stacks; keep the production flags.
cflags="$cflags -g -fno-omit-frame-pointer $VG_INC_FLAG"
srcs=$(audit_sign_srcs "$BACKEND" "$MODE" "$XOF")
DIR=$ROOT/$BACKEND
cd "$DIR"
mkdir -p out
bin=out/timecop_smoke_${BACKEND}_$MODE
# shellcheck disable=SC2086
$cc $cflags "$HARNESS" $srcs -lm -o "$bin"

set +e
"$VG" -q --variable-latency-errors=yes "./$bin" > "$bin.full.out" 2> "$bin.full.err"
"$VG" -q --variable-latency-errors=yes --suppressions="$SUPP" "./$bin" > "$bin.out" 2> "$bin.err"
rc=$?
set -e

if grep -qiE 'SIGILL|Illegal opcode|unhandled instruction|disInstr' "$bin.full.err"; then
  why=$(grep -m1 -iE 'Illegal opcode|unhandled instruction|disInstr' "$bin.full.err" | sed 's/^==[0-9]*== *//')
  echo "timecop $BACKEND mode-$MODE smoke: SKIP - Valgrind cannot execute this backend (${why:-SIGILL}); covered statically by ct_scan + dudect-sign"
  exit 3
fi

varlat=$(grep -c 'Variable-latency instruction operand' "$bin.err" || true)
cond_full=$(grep -c 'Conditional jump or move depends' "$bin.full.err" || true)
value_full=$(grep -c 'Use of uninitialised value' "$bin.full.err" || true)

if [ "$varlat" -ne 0 ]; then
  echo "FAIL: SHUTTLE $BACKEND mode-$MODE smoke reported variable-latency secret operand" >&2
  grep -A8 'Variable-latency instruction operand' "$bin.err" >&2
  exit 1
fi
if [ "$rc" -ne 0 ]; then
  echo "FAIL: $BACKEND mode-$MODE smoke binary exited rc=$rc" >&2
  cat "$bin.out" >&2; cat "$bin.err" >&2
  exit 1
fi
{
  echo
  echo "## SHUTTLE $BACKEND mode-$MODE Smoke"
  echo
  echo "- Valgrind: \`$VG\`"
  echo "- Binary: \`$BACKEND/$bin\` (\`$(cat "$bin.out")\`)"
  echo "- Flags: \`--variable-latency-errors=yes --suppressions=tools/timecop/shuttle-ct.supp\`"
  echo "- Variable-latency secret operands: **$varlat** (gate: must be 0) -> PASS"
  echo "- Out-of-scope intentional-CT reports (unsuppressed): $cond_full \`Memcheck:Cond\`, $value_full \`Memcheck:Value\` (declassified output + branchless CT idioms; owned by ct_scan + dudect-sign)."
} >> "$ROOT/docs/design-notes/TIMECOP.md"
echo "timecop $BACKEND mode-$MODE smoke: PASS (variable-latency operands=$varlat; out-of-scope cond=$cond_full value=$value_full)"
