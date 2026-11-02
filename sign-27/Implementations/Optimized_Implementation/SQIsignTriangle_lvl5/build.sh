#!/usr/bin/env sh
set -eu
cd "$(dirname "$0")"
# ENABLE_STRICT=OFF keeps the build robust across compiler versions (the
# upstream -Werror set can trip newer GCCs on the reference integer code).
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DENABLE_STRICT=OFF
cmake --build build --target KAT_SIG_SQIsignTriangle_lvl5 -j
