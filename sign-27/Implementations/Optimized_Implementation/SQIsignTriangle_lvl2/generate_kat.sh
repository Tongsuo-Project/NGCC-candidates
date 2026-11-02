#!/usr/bin/env sh
set -eu
cd "$(dirname "$0")"
./build.sh
./build/KAT_SIG_SQIsignTriangle_lvl2
mkdir -p ../../../Test_Vectors
cp output/KAT_SIG_SQIsignTriangle_lvl2.txt ../../../Test_Vectors/
