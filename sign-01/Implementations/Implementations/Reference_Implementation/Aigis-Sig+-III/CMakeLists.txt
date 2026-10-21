cmake_minimum_required(VERSION 3.13)

project(aigis_sig_ref C)

# Keep close mapping to Makefile for easier bidirectional conversion.
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Makefile: CFLAGS = -Wall -Wextra -march=native -mtune=native -fomit-frame-pointer -O3
add_compile_options(-Wall -Wextra -march=native -mtune=native -fomit-frame-pointer -O3)

set(SOURCES
    sign.c
    polyvec.c
    packing.c
    poly.c
    reduce.c
    ntt.c
    rounding.c
    fips202.c
    hashkdf.c
    auxfunc.c
    SIG_AlgorithmInstance.c
    drng.c
    pspm.c
)

set(HEADERS
    api.h
    params.h
    polyvec.h
    packing.h
    poly.h
    reduce.h
    ntt.h
    pspm.h
    rounding.h
    fips202.h
    hashkdf.h
    auxfunc.h
    SIG_AlgorithmInstance.h
    drng.h
    randombytes.h
)

set(RANDOM
    randombytes.c
)

set(DRANDOM
    rng.c
)

add_executable(speed128_sha3
    ${SOURCES}
    ${RANDOM}
    aigis_sig.c
    speed.c
    cpucycles.c
    ${HEADERS}
    speed.h
    cpucycles.h
)
target_compile_definitions(speed128_sha3 PRIVATE PARAMS=1 USE_SHA3)

add_executable(speed256_sha3
    ${SOURCES}
    ${RANDOM}
    aigis_sig.c
    speed.c
    cpucycles.c
    ${HEADERS}
    speed.h
    cpucycles.h
)
target_compile_definitions(speed256_sha3 PRIVATE PARAMS=2 USE_SHA3)

add_executable(speed512_sha3
    ${SOURCES}
    ${RANDOM}
    aigis_sig.c
    speed.c
    cpucycles.c
    ${HEADERS}
    speed.h
    cpucycles.h
)
target_compile_definitions(speed512_sha3 PRIVATE PARAMS=3 USE_SHA3)

add_executable(PQCgenKAT_sign128_sha3
    KAT_SIG.c
    ${SOURCES}
    ${HEADERS}
    ${DRANDOM}
)
target_compile_definitions(PQCgenKAT_sign128_sha3 PRIVATE PARAMS=1 USE_SHA3)

add_executable(PQCgenKAT_sign256_sha3
    KAT_SIG.c
    ${SOURCES}
    ${HEADERS}
    ${DRANDOM}
)
target_compile_definitions(PQCgenKAT_sign256_sha3 PRIVATE PARAMS=2 USE_SHA3)

add_executable(PQCgenKAT_sign512_sha3
    KAT_SIG.c
    ${SOURCES}
    ${HEADERS}
    ${DRANDOM}
)
target_compile_definitions(PQCgenKAT_sign512_sha3 PRIVATE PARAMS=3 USE_SHA3)

add_executable(PQCgenKAT_sign128_iccs
    KAT_SIG.c
    ${SOURCES}
    ${HEADERS}
    ${DRANDOM}
)
target_compile_definitions(PQCgenKAT_sign128_iccs PRIVATE PARAMS=1 USE_ICCS)

add_executable(PQCgenKAT_sign256_iccs
    KAT_SIG.c
    ${SOURCES}
    ${HEADERS}
    ${DRANDOM}
)
target_compile_definitions(PQCgenKAT_sign256_iccs PRIVATE PARAMS=2 USE_ICCS)

add_executable(PQCgenKAT_sign512_iccs
    KAT_SIG.c
    ${SOURCES}
    ${HEADERS}
    ${DRANDOM}
)
target_compile_definitions(PQCgenKAT_sign512_iccs PRIVATE PARAMS=3 USE_ICCS)

# CMake clean removes build artifacts; keep a Makefile-like target for generated txt files.
add_custom_target(clean_txt
    COMMAND ${CMAKE_COMMAND} -E rm -f ${CMAKE_CURRENT_SOURCE_DIR}/*.txt
    COMMENT "Remove generated txt files (Makefile clean compatibility)"
)
