#!/usr/bin/env sh
set -eu
cd "$(dirname "$0")"
./build.sh
./build/KAT_SIG_SQIsignTriangle_lvl5
mkdir -p ../../../Test_Vectors
cp output/KAT_SIG_SQIsignTriangle_lvl5.txt ../../../Test_Vectors/
