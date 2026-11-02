#!/bin/bash
# Generate reference KAT files for the NEW levels (lvl2, lvl6) only, leaving the
# existing lvl1/3/5 references untouched. Run from the repo root after building
# (build/apps/PQCgenKAT_sign_lvl{2,6} must exist).
set -e

echo 'Generating KAT for Level 2...'
./build/apps/PQCgenKAT_sign_lvl2
mv PQCsignKAT_437_SQIsign_lvl2.req ./KAT/PQCsignKAT_437_SQIsign_lvl2.req
mv PQCsignKAT_437_SQIsign_lvl2.rsp ./KAT/PQCsignKAT_437_SQIsign_lvl2.rsp

echo 'Generating KAT for Level 6 (may take a few minutes; 1024-bit signing)...'
./build/apps/PQCgenKAT_sign_lvl6
mv PQCsignKAT_1409_SQIsign_lvl6.req ./KAT/PQCsignKAT_1409_SQIsign_lvl6.req
mv PQCsignKAT_1409_SQIsign_lvl6.rsp ./KAT/PQCsignKAT_1409_SQIsign_lvl6.rsp

echo 'Done. KAT files for lvl2/lvl6 written to KAT/. Re-run: (cd build && ctest -R "lvl2_KAT|lvl6_KAT")'
