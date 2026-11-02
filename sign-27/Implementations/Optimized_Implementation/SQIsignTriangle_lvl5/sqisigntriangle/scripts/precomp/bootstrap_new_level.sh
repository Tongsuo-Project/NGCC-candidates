#!/usr/bin/env bash
# Generate the per-level precomputed constants (.c/.h) for one SQIsign level by
# running the eight SageMath precompute scripts with the level's precomp dir as
# the working directory (mirrors what `make precomp` does, but works for a fresh
# level that has not been configured/built yet).
#
# Usage:   bash scripts/precomp/bootstrap_new_level.sh lvl2
# Requires: SageMath >= 10.5 in PATH (the `sage` command).
set -euo pipefail
LVL="${1:?usage: bootstrap_new_level.sh lvlN}"
REPO="$(cd "$(dirname "$0")/../.." && pwd)"
DIR="$REPO/src/precomp/ref/$LVL"
SCR="$REPO/scripts/precomp"
[ -f "$DIR/sqisign_parameters.txt" ] || { echo "no $DIR/sqisign_parameters.txt"; exit 1; }
mkdir -p "$DIR/include"
cd "$DIR"
for s in precompute_torsion_constants precompute_quaternion_constants precompute_sizes \
         precompute_quaternion_data precompute_endomorphism_action ec_params \
         precompute_hd_splitting precompute_E0_basis; do
  echo "=== running $s.sage  (cwd=$DIR) ==="
  sage "$SCR/$s.sage"
done
echo "Done. Generated constants for $LVL in $DIR (+ include/, + ../../../nistapi/$LVL/api.h)."
