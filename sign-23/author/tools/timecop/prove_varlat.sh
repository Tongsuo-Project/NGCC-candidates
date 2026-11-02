#!/bin/sh
# Positive control: an intentional secret division MUST be flagged by the
# patched Valgrind, and a public (constant) division must NOT be. Proves the
# variable-latency detector is live before the SHUTTLE signing smoke trusts it.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
VG=
while [ $# -gt 0 ]; do
  case "$1" in
    --valgrind) VG=$2; shift 2;;
    *) echo "usage: $0 --valgrind PATH" >&2; exit 2;;
  esac
done
[ -n "$VG" ] || { echo "usage: $0 --valgrind PATH" >&2; exit 2; }
WORK=${TIMECOP_PROBE_DIR:-$ROOT/external/timecop/probes}
mkdir -p "$WORK" "$ROOT/docs/design-notes"
cat > "$WORK/secret_div.c" <<'C'
#include <stdlib.h>
volatile int storage;
static void secret_div(char c) { storage = 100 / (13 | c); }
int main(void) {
    volatile char *x = (volatile char *)malloc(1);
    secret_div(x[0]);
    free((void *)x);
    return 0;
}
C
cat > "$WORK/public_div.c" <<'C'
volatile int storage;
int main(void) { storage = 100 / 13; return 0; }
C
cc -O2 "$WORK/secret_div.c" -o "$WORK/secret_div"
cc -O2 "$WORK/public_div.c" -o "$WORK/public_div"
# NOTE: --variable-latency-errors rides on Memcheck's undefined-value
# instrumentation, so --undef-value-errors must stay at its default (yes).
set +e
"$VG" -q --variable-latency-errors=yes "$WORK/secret_div" > "$WORK/secret_div.out" 2> "$WORK/secret_div.err"
pos_rc=$?
"$VG" -q --variable-latency-errors=yes "$WORK/public_div" > "$WORK/public_div.out" 2> "$WORK/public_div.err"
neg_rc=$?
set -e
if ! grep -q 'Variable-latency instruction operand' "$WORK/secret_div.err"; then
  echo "FAIL: secret division was not reported" >&2
  cat "$WORK/secret_div.err" >&2
  exit 1
fi
if grep -q 'Variable-latency instruction operand' "$WORK/public_div.err"; then
  echo "FAIL: public division unexpectedly reported" >&2
  cat "$WORK/public_div.err" >&2
  exit 1
fi
{
  echo
  echo "## Tiny Secret-Division Probe"
  echo
  echo "- Valgrind: \`$VG\`"
  echo "- Positive rc: \`$pos_rc\`; reported variable-latency operand."
  echo "- Negative rc: \`$neg_rc\`; no variable-latency report."
} >> "$ROOT/docs/design-notes/TIMECOP.md"
echo "prove_varlat: PASS"
