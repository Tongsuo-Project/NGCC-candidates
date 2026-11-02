#!/bin/sh
# One-shot SHUTTLE TIMECOP driver: ensure a patched variable-latency Valgrind
# exists, run the positive-control secret-division probe, then the SHUTTLE
# signing smoke for each runnable backend x parameter mode. OPT-IN dynamic-taint
# cross-check; NOT part of the hard gate (needs a local patched-Valgrind build +
# network on first run).
#
#   tools/timecop/run_all.sh                        # build-if-needed, then probes
#   TIMECOP_BACKENDS="ref avx2" TIMECOP_MODES="128 256 512" tools/timecop/run_all.sh
#   TIMECOP_TRY_AVX512=1 tools/timecop/run_all.sh    # attempt avx512 (will SKIP: SIGILL)
#   TIMECOP_VALGRIND=/path/to/valgrind tools/timecop/run_all.sh
#
# ref and avx2 run under Valgrind; avx512 cannot (Valgrind 3.23 SIGILLs on
# EVEX/VBMI2), so it is reported as SKIP and covered statically by ct_scan +
# dudect-sign.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
WORK=${TIMECOP_WORKDIR:-$ROOT/external/timecop/valgrind-varlat}
VG=${TIMECOP_VALGRIND:-$WORK/install/bin/valgrind}

has_varlat() {
  [ -x "$1" ] && "$1" --help 2>&1 | grep -q -- '--variable-latency-errors'
}

if ! has_varlat "$VG"; then
  echo "== patched Valgrind not found; building (one-time, downloads + compiles) =="
  JOBS=${JOBS:-$(nproc 2>/dev/null || echo 2)} sh "$ROOT/tools/timecop/build_valgrind_varlat.sh"
  VG=$WORK/install/bin/valgrind
fi
has_varlat "$VG" || { echo "FAIL: no usable variable-latency Valgrind at $VG" >&2; exit 1; }

echo "== TIMECOP Valgrind: $("$VG" --version) ($VG) =="
sh "$ROOT/tools/timecop/prove_varlat.sh" --valgrind "$VG"
for be in ${TIMECOP_BACKENDS:-ref avx2}; do
  for mode in ${TIMECOP_MODES:-128 256 512}; do
    sh "$ROOT/tools/timecop/run_sign_smoke.sh" --valgrind "$VG" --backend "$be" --mode "$mode"
  done
done
if [ "${TIMECOP_TRY_AVX512:-0}" = 1 ]; then
  for mode in ${TIMECOP_MODES:-128 256 512}; do
    sh "$ROOT/tools/timecop/run_sign_smoke.sh" --valgrind "$VG" --backend avx512 --mode "$mode" \
      || { rc=$?; [ "$rc" = 3 ] || exit "$rc"; }
  done
else
  echo "timecop avx512: SKIP - Valgrind 3.23 cannot execute AVX-512 (SIGILL); covered statically by ct_scan + dudect-sign. Set TIMECOP_TRY_AVX512=1 to demonstrate."
fi
echo "timecop run_all: PASS"
