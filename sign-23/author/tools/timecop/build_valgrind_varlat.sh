#!/bin/sh
# Build a KyberSlash-style Valgrind variable-latency Memcheck locally (SHUTTLE).
#
# The KyberSlash valgrind-varlat patch is cut from the Valgrind *git* tree, so
# its first hunk touches `.gitignore` -- a file the released source tarballs do
# NOT ship. We filter the patch to drop sections that MODIFY a file absent from
# the extracted tree (keeping new-file creations), apply the remainder with
# fuzz, and judge success by capability (`--variable-latency-errors` present and
# a working build) rather than by patch's raw exit code.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
WORK=${TIMECOP_WORKDIR:-$ROOT/external/timecop/valgrind-varlat}
PATCH_20240808=${PATCH_20240808:-https://kyberslash.cr.yp.to/valgrind-varlat-patch-20240808.txt}
PATCH_20250805=${PATCH_20250805:-https://kyberslash.cr.yp.to/valgrind-varlat-patch-20250805.txt}
# 3.23.0 is the native target of the 20240808 patch (smallest hunk offsets).
VERSIONS=${VALGRIND_VERSIONS:-3.23.0 3.22.0}
mkdir -p "$WORK/src" "$WORK/downloads" "$WORK/install" "$ROOT/docs/design-notes"
LOG="$WORK/build.log"
: > "$LOG"
log() { printf '%s\n' "$*" | tee -a "$LOG"; }
fetch() {
  url=$1; out=$2
  [ -f "$out" ] || curl -L --fail --retry 3 -o "$out" "$url"
}

# filter_patch SRC_DIR < raw.patch > filtered.patch
filter_patch() {
  awk -v srcdir="$1" '
    function flush() {
      if (have) { if (keep) for (i = 0; i < n; i++) print buf[i]; }
      have = 0; n = 0; keep = 1; isnew = 0;
    }
    /^diff --git / { flush(); have = 1; buf[n++] = $0; next }
    /^new file mode/ { if (have) { isnew = 1; buf[n++] = $0; next } }
    /^--- / {
      if (have) {
        buf[n++] = $0;
        if ($0 == "--- /dev/null" || isnew) { keep = 1 }
        else {
          f = $0; sub(/^--- a\//, "", f); sub(/^--- /, "", f);
          cmd = "test -e \"" srcdir "/" f "\""; keep = (system(cmd) == 0);
        }
        next;
      }
    }
    { if (have) buf[n++] = $0; else print }
    END { flush() }
  '
}

try_build() {
  ver=$1
  src_tgz="$WORK/downloads/valgrind-$ver.tar.bz2"
  src_url="https://sourceware.org/pub/valgrind/valgrind-$ver.tar.bz2"
  patch_a="$WORK/downloads/valgrind-varlat-patch-20240808.txt"
  patch_b="$WORK/downloads/valgrind-varlat-patch-20250805.txt"
  fetch "$src_url" "$src_tgz"
  fetch "$PATCH_20240808" "$patch_a"
  fetch "$PATCH_20250805" "$patch_b"
  rm -rf "$WORK/src/valgrind-$ver"
  tar -C "$WORK/src" -xf "$src_tgz"
  src="$WORK/src/valgrind-$ver"
  cd "$src"
  chosen_patch=
  for cand in "$patch_a:$PATCH_20240808" "$patch_b:$PATCH_20250805"; do
    pf=${cand%%:*}
    purl=${cand#*:}
    filt="$WORK/downloads/$(basename "$pf" .txt).filtered-$ver.txt"
    filter_patch "$src" < "$pf" > "$filt"
    if patch -p1 -F 3 --dry-run < "$filt" >> "$LOG" 2>&1; then
      patch -p1 -F 3 --no-backup-if-mismatch < "$filt" >> "$LOG" 2>&1
      chosen_patch=$purl
      log "applied $(basename "$pf") (filtered) to Valgrind $ver"
      break
    fi
    log "filtered $(basename "$pf") does not apply to Valgrind $ver"
  done
  [ -n "$chosen_patch" ] || { log "no candidate patch applies to Valgrind $ver"; return 1; }
  grep -rq 'variable-latency-errors' memcheck/ || { log "patched source lacks variable-latency option"; return 1; }
  find . -name 'Makefile.in' -exec touch {} + 2>/dev/null || true
  find . -name 'configure' -exec touch {} + 2>/dev/null || true
  ./configure --prefix="$WORK/install" >> "$LOG" 2>&1
  make -j"${JOBS:-$(nproc 2>/dev/null || echo 2)}" >> "$LOG" 2>&1
  make install >> "$LOG" 2>&1
  "$WORK/install/bin/valgrind" --help 2>&1 | grep -q -- '--variable-latency-errors' || return 1
  cat > "$WORK/BUILD_INFO" <<EOF
version=$ver
patch=$chosen_patch
valgrind=$WORK/install/bin/valgrind
log=$LOG
EOF
  log "PASS built patched Valgrind $ver with $chosen_patch"
  return 0
}
for ver in $VERSIONS; do
  log "== trying Valgrind $ver =="
  if try_build "$ver"; then
    info=$(cat "$WORK/BUILD_INFO")
    {
      echo
      echo "## Build Result"
      echo
      echo '```'
      printf '%s\n' "$info"
      echo '```'
    } >> "$ROOT/docs/design-notes/TIMECOP.md"
    exit 0
  fi
  log "FAIL Valgrind $ver"
done
log "no supported patched Valgrind build completed"
exit 1
