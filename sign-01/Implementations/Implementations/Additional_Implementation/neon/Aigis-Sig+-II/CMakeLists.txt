cmake_minimum_required(VERSION 3.10)
project(Aigis_sig_avx2 C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(COMMON_SOURCES
    sign.c
    polyvec.c
    packing.c
    poly.c
    ntt.c
    fips202.c
    hashkdf.c
    pspm.c
    fips202x4.c
    auxfunc.c
    SIG_AlgorithmInstance.c
    drng.c
    sample.c
)

set(COMMON_HEADERS
    api.h
    params.h
    sign.h
    polyvec.h
    packing.h
    poly.h
    ntt.h
    pspm.h
    sample.h
    align.h
    fips202.h
    fips202x4.h
    hashkdf.h
    auxfunc.h
    SIG_AlgorithmInstance.h
    drng.h
    sm3kdf.h
    randombytes.h
)

set(KECCAK4X_HEADERS
    keccak4x/KeccakP-1600-times4-SIMD256.c
    keccak4x/align.h
    keccak4x/brg_endian.h
    keccak4x/KeccakP-1600-times4-SnP.h
    keccak4x/KeccakP-1600-unrolling.macros
    keccak4x/SIMD256-config.h
)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -march=native -mtune=native -O3 -fomit-frame-pointer -mpopcnt -g")

add_library(keccak4x_obj OBJECT ${KECCAK4X_HEADERS})
target_include_directories(keccak4x_obj PRIVATE keccak4x)
target_compile_options(keccak4x_obj PRIVATE -Wall -Wextra -march=native -mtune=native -O3 -fomit-frame-pointer -mpopcnt -g)

set(KECCAK4X_OBJECTS $<TARGET_OBJECTS:keccak4x_obj>)

add_executable(speed128
    ${COMMON_SOURCES}
    randombytes.c
    aigis_sig.c
    speed.c
    cpucycles.c
    ${KECCAK4X_OBJECTS}
    ${COMMON_HEADERS}
    speed.h
    cpucycles.h
)
target_compile_definitions(speed128 PRIVATE PARAMS=1)

add_executable(speed256
    ${COMMON_SOURCES}
    randombytes.c
    aigis_sig.c
    speed.c
    cpucycles.c
    ${KECCAK4X_OBJECTS}
    ${COMMON_HEADERS}
    speed.h
    cpucycles.h
)
target_compile_definitions(speed256 PRIVATE PARAMS=2)

add_executable(speed512
    ${COMMON_SOURCES}
    randombytes.c
    aigis_sig.c
    speed.c
    cpucycles.c
    ${KECCAK4X_OBJECTS}
    ${COMMON_HEADERS}
    speed.h
    cpucycles.h
)
target_compile_definitions(speed512 PRIVATE PARAMS=3)

add_executable(PQCgenKAT_sign128
    KAT_SIG.c
    ${COMMON_SOURCES}
    rng.c
    ${KECCAK4X_OBJECTS}
    ${COMMON_HEADERS}
)
target_compile_definitions(PQCgenKAT_sign128 PRIVATE PARAMS=1)

add_executable(PQCgenKAT_sign256
    KAT_SIG.c
    ${COMMON_SOURCES}
    rng.c
    ${KECCAK4X_OBJECTS}
    ${COMMON_HEADERS}
)
target_compile_definitions(PQCgenKAT_sign256 PRIVATE PARAMS=2)

add_executable(PQCgenKAT_sign512
    KAT_SIG.c
    ${COMMON_SOURCES}
    rng.c
    ${KECCAK4X_OBJECTS}
    ${COMMON_HEADERS}
)
target_compile_definitions(PQCgenKAT_sign512 PRIVATE PARAMS=3)
