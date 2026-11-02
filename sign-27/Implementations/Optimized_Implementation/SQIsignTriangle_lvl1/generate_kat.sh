#!/usr/bin/env sh
set -eu
cd "$(dirname "$0")"
./build.sh
./build/KAT_SIG_SQIsignTriangle_lvl1
mkdir -p ../../../Test_Vectors
cp output/KAT_SIG_SQIsignTriangle_lvl1.txt ../../../Test_Vectors/
