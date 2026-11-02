#!/bin/sh
# audit_lib.sh -- shared build recipe for the security audit drivers.
#
# The audit harness owns DISJOINT new files and does NOT edit the per-backend
# Makefiles, so the audit scripts compile their own self-contained driver
# binaries here (using the same flags the production Makefiles use). Sourced by
# security_audit_matrix.sh / integration_audit_matrix.sh / run_dudect_*.sh.
#
# Usage (POSIX sh):
#   . "$(dirname "$0")/tools/audit_lib.sh"
#   audit_build <backend> <mode> <xof_mode> <driver_src> <out_bin> [extra_cflags...]
#     backend  : ref | avx2 | avx512
#     mode     : 128 | 256 | 512
#     xof_mode : ngcc | sha3
#     driver_src: absolute or SHUTTLE-relative path to the driver .c
#     out_bin  : output binary path (created under <backend>/out by convention)
# Returns 0 on success; prints the cc line + errors on failure.

# SHUTTLE root = directory containing this tools/ dir's parent.
AUDIT_ROOT=$(CDPATH= cd -- "$(dirname "$0")" && pwd)

audit_qset() {
    case "$1" in
        128) echo q15361n256 ;;
        256) echo q61441n512 ;;
        512) echo q59393n1024 ;;
        *) echo "audit_lib: bad mode $1" >&2; return 1 ;;
    esac
}

# The full secret-handling sign source list (scheme spine), per backend. The
# NTT vendoring + XOF backend are appended per (backend, mode, xof).
audit_sign_srcs() {
    backend=$1; mode=$2; xof=$3
    qs=$(audit_qset "$mode") || return 1
    # Scheme spine (symlinked scalar in avx2/avx512; poly_ntt.c is the SIMD shim
    # there). Identical file list across backends (the single-source design).
    spine="sign.c polyvec.c sampler.c sampler_u.c irs.c rounding.c packing.c poly.c poly_ntt.c reduce.c rans.c approx_exp.c approx_log.c symmetric.c"
    # XOF backend.
    if [ "$xof" = sha3 ]; then
        # SHA3 scheme XOF + the SM3 DRBG that the KAT/_xi seeding always uses.
        xof_srcs="fips202.c drng.c auxfunc.c"
    else
        xof_srcs="drng.c auxfunc.c"
    fi
    # Per-backend NTT vendoring.
    case "$backend" in
        ref)
            ntt="ntt/$qs/ntt_ref.c" ;;
        avx2)
            ntt="$qs/ntt.S $qs/ntt_consts.c ../ref/ntt/$qs/ntt_ref.c" ;;
        avx512)
            ntt="$qs/ntt_avx512.S $qs/ntt_consts_avx512.c ../ref/ntt/$qs/ntt_ref.c" ;;
    esac
    echo "$spine $xof_srcs $ntt"
}

audit_cflags() {
    backend=$1; mode=$2; xof=$3
    qs=$(audit_qset "$mode") || return 1
    common="-O2 -std=c99 -DDISABLE_NAMESPACE=1 -DSHUTTLE_MODE=$mode -I../tools"
    [ "$xof" = sha3 ] && common="$common -DSHA3_MODE"
    case "$backend" in
        ref)
            echo "$common -I. -Itest -Intt/$qs" ;;
        avx2)
            echo "$common -I. -I../ref -I../ref/test -I../ref/ntt/$qs -mavx2 -mbmi2 -mpopcnt -march=x86-64 -mtune=native -mno-avx512f -DUSE_AVX2_NTT -DUSE_AVX2_SAMPLER -DUSE_AVX2_SHAKE4X" ;;
        avx512)
            echo "$common -I. -I../ref -I../ref/test -I../ref/ntt/$qs -mavx2 -mbmi2 -mpopcnt -march=x86-64 -mtune=native -mavx512f -mavx512bw -mavx512dq -mavx512vl -mavx512vbmi2 -DUSE_AVX512_NTT -DUSE_AVX512_SAMPLER -DUSE_AVX512_SHAKE8X" ;;
    esac
}

# audit_build backend mode xof driver_src out_bin [extra...]
# NOTE: variables are deliberately uniquely named (ab_*) because POSIX sh has no
# function-local scope -- a generic name like `out` would clobber the caller's.
audit_build() {
    ab_backend=$1; ab_mode=$2; ab_xof=$3; ab_drv=$4; ab_obin=$5; shift 5
    ab_extra="$*"
    ab_cc=${CC:-gcc}
    ab_cflags=$(audit_cflags "$ab_backend" "$ab_mode" "$ab_xof") || return 1
    ab_srcs=$(audit_sign_srcs "$ab_backend" "$ab_mode" "$ab_xof") || return 1
    case "$ab_drv" in
        /*) ab_drvpath=$ab_drv ;;
        *) ab_drvpath="$AUDIT_ROOT/$ab_drv" ;;
    esac
    ab_bdir="$AUDIT_ROOT/$ab_backend"
    ( cd "$ab_bdir" && mkdir -p out && \
      # shellcheck disable=SC2086
      $ab_cc $ab_cflags $ab_extra "$ab_drvpath" $ab_srcs -lm -o "$ab_obin" )
}
