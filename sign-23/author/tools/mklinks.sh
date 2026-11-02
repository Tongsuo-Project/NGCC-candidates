#!/bin/sh
# mklinks.sh - materialize the avx2/ and avx512/ backend trees by relative
# symlinking the SHARED scalar source from ref/, skipping the per-backend FORK
# set.
#
# ref/ is the single source of truth.  At the byte-exactness milestone the
# ONLY genuine forks/AVX-only files per backend are:
#   - config.h        (real fork: _avx2 / _avx512 namespace tail)
#   - poly_ntt.c      (real SIMD fork: hard-codes the AVX NTT signature families
#                      + signed canonicalization; the scalar ref/poly_ntt.c is
#                      NOT symlinked)
#   - the AVX-only / N-way XOF files: fips202x4/x8.*, f1600x4.S,
#     keccakf1600x8.c, symmetric_avx2/512.c, the SM3 N-way auxfunc_avx2/512.* +
#     drng_avx2/512.*, sm3_const.h / gen_sm3_const.py
#   - the per-qset vendored NTT asm dirs (q15361n256/, q61441n512/,
#     q59393n1024/ -- handled as directories, never file-by-file)
#
# EVERYTHING else is a relative symlink `ln -s ../ref/<f>` -- including the
# would-be-fork scalar sources (sampler.c polyvec.c irs.c rounding.c sign.c)
# and the NGCC infra (SIG_AlgorithmInstance.{c,h}, KAT_SIG.c, drng.{c,h},
# auxfunc.{c,h}, README.txt).  At byte-exactness the backends = scalar scheme
# logic + SIMD NTT (poly_ntt.c fork + vendored asm) + SIMD XOF (the N-way
# files); the SIMD sampler/polyvec/rounding perf kernels are a LATER
# milestone.  Until then
# the symlinked scalar .c is the running implementation, and -DUSE_AVX2_* /
# -DUSE_AVX512_* only route the NTT shim (poly_ntt.c) and the (currently
# unbuilt) SIMD kernels; the scalar bodies are byte-exact, so symlinking them
# is what makes the avx2/avx512 KAT hash equal the ref hash.
#
# This script is IDEMPOTENT and re-runnable: it (re)creates only the symlinks
# for shared files that actually exist in ref/ right now and are not in the
# fork/skip set, and never clobbers a real (non-symlink) file in a backend dir.
# As later plans add shared sources to ref/, re-running this picks them up.
#
# Run from anywhere:  sh tools/mklinks.sh   (or: cd tools && sh mklinks.sh)

set -eu

# Resolve the SHUTTLE root from this script's location (tools/..).
here=$(CDPATH= cd "$(dirname "$0")" && pwd)
root=$(CDPATH= cd "$here/.." && pwd)
ref="$root/ref"

# --- FORK / SKIP set: real files per backend, never symlinked from ref/. ---
# At byte-exactness this is ONLY the genuine forks + AVX-only files (see
# header).  The would-be-fork scalar sources (polyvec/sampler/irs/rounding/sign)
# and ALL the NGCC infra (SIG_AlgorithmInstance/KAT_SIG/drng/auxfunc/README) are
# SYMLINKED scalar -- they are NOT in this skip set.
# is_fork <filename> <backend>: true (return 0) iff <filename> is a REAL fork
# in <backend>/ and must NOT be symlinked from ref/.  Most forks are common to
# all backends; the SIMD sampler perf forks (sampler.c, polyvec.c) exist for
# BOTH the avx2 and avx512 SIMD backends (each carries its own width-specific
# kernel), so they are backend-gated to {avx2,avx512}.
is_fork() {
    backend="$2"
    # SIMD sampler perf forks (avx2 + avx512): sampler.c routes the 96-bit
    # RCDT scan (cdt_scan96) through a SIMD borrow-fold kernel (avx2 = 8-way
    # flip-to-signed; avx512 = 16-way native unsigned vpcmpltud), and polyvec.c
    # routes the ExpandA uniform-reject scan through a vectorized reject +
    # compaction (avx2 = 16-wide + BMI2 pdep/pext; avx512 = 32-wide + vpcompressw).
    # Both are guarded by -DUSE_AVX2_SAMPLER / -DUSE_AVX512_SAMPLER and are
    # BYTE-EXACT to the scalar ref (KAT-locked).
    # SIMD NTT commitment fork (avx2 + avx512): rounding.c routes the
    # mat_mul_2q / mat_mul_z1_2q NTT-domain products through the SIMD NTT
    # kernels (poly_ntt_simd / *_import / pointwise / invntt), importing the
    # cached canonical operands to backend-native order via nttunpack.
    # Guarded by -DUSE_AVX2_NTT / -DUSE_AVX512_NTT and BYTE-EXACT to the
    # scalar ref (KAT-locked: the mod-2q lift downstream is unchanged scalar).
    if [ "$backend" = "avx2" ] || [ "$backend" = "avx512" ]; then
        case "$1" in
        sampler.c | polyvec.c | rounding.c) return 0 ;;
        esac
    fi
    case "$1" in
    # Real per-backend fork: the namespace-tail-only config.h.
    config.h) return 0 ;;
    # The NTT shim has a per-backend FORK body (avx2/avx512 poly_ntt.c
    # hard-code the two asm signature families + signed canonicalization); the
    # scalar ref/poly_ntt.c is NOT symlinked into the backends.
    poly_ntt.c) return 0 ;;
    # AVX-only / N-way XOF files are real in the backend dirs.
    fips202x4.* | fips202x8.* | f1600x4.* | keccakf1600x8.* | ntt_avx_decls.h) return 0 ;;
    symmetric_avx2.* | symmetric_avx512.*) return 0 ;;
    auxfunc_avx2.* | auxfunc_avx512.* | drng_avx2.* | drng_avx512.*) return 0 ;;
    sm3_const.h | gen_sm3_const.py) return 0 ;;
    # build/dir artifacts and the test dir are not symlinked file-by-file
    # (the per-qset NTT asm dirs q15361n256/ etc. are directories, skipped by
    # the `-f` regular-file test in link_into).
    Makefile | out | test | explore | .gitignore | README.md) return 0 ;;
    *) return 1 ;;
    esac
}

link_into() {
    backend="$1"
    dir="$root/$backend"
    mkdir -p "$dir"
    n=0
    for path in "$ref"/*; do
        f=$(basename "$path")
        # Only link regular files (skip directories like test/ out/).
        [ -f "$path" ] || continue
        if is_fork "$f" "$backend"; then
            continue
        fi
        target="../ref/$f"
        dest="$dir/$f"
        if [ -L "$dest" ]; then
            # already a symlink: refresh it (idempotent).
            ln -sf "$target" "$dest"
            n=$((n + 1))
        elif [ -e "$dest" ]; then
            # a REAL file lives here (a fork we didn't list, or stale): leave it.
            echo "  skip (real file present): $backend/$f"
        else
            ln -s "$target" "$dest"
            n=$((n + 1))
        fi
    done
    # Mirror the test/ source dir: backends build `test/<t>.c` (resolved in the
    # backend dir), so symlink each ref/test/*.c|*.h into <backend>/test/.  The
    # backends fork no test sources, so all of them are symlinked.
    if [ -d "$ref/test" ]; then
        mkdir -p "$dir/test"
        for tpath in "$ref"/test/*; do
            [ -f "$tpath" ] || continue
            tf=$(basename "$tpath")
            tdest="$dir/test/$tf"
            if [ -L "$tdest" ]; then
                ln -sf "../../ref/test/$tf" "$tdest"
            elif [ -e "$tdest" ]; then
                echo "  skip (real file present): $backend/test/$tf"
            else
                ln -s "../../ref/test/$tf" "$tdest"
            fi
        done
    fi
    echo "$backend: linked/refreshed $n shared file(s) + test/ from ref/"
}

link_into avx2
link_into avx512
echo "mklinks.sh: done"
