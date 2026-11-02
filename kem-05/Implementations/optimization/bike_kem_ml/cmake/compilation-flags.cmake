# Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
# SPDX-License-Identifier: Apache-2.0

if(CMAKE_C_COMPILER_ID MATCHES "Clang")
  set(CLANG 1)
else()
  set(CLANG 0)
endif()

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ggdb -fPIC -std=c99")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fvisibility=hidden -Wall -Wextra -Werror -Wpedantic")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wunused -Wcomment -Wchar-subscripts -Wuninitialized -Wshadow")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wwrite-strings -Wformat-security -Wcast-qual -Wunused-result")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -funroll-loops")

if(X86_64)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -m64 -mno-red-zone")
elseif(X86)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -m32 -mno-red-zone")
endif()

# Avoiding GCC 4.8 bug
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-missing-braces -Wno-missing-field-initializers")

if(NOT DEFINED VERBOSE)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wcast-align")
endif()

if(MSAN)
  if(NOT CLANG)
    message(FATAL_ERROR "Cannot enable MSAN unless using Clang")
  endif()

  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=memory -fsanitize-memory-track-origins -fno-omit-frame-pointer")
endif()

if(ASAN)
  if(NOT CLANG)
    message(FATAL_ERROR "Cannot enable ASAN unless using Clang")
  endif()

  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address -fsanitize-address-use-after-scope -fno-omit-frame-pointer")
endif()

if(TSAN)
  if(NOT CLANG)
    message(FATAL_ERROR "Cannot enable TSAN unless using Clang")
  endif()

  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=thread")
endif()

if(UBSAN)
  if(NOT CLANG)
    message(FATAL_ERROR "Cannot enable UBSAN unless using Clang")
  endif()

  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=undefined")
endif()

if(RDTSC)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DRDTSC")
endif()

if(VERBOSE)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DVERBOSE=${VERBOSE}")
endif()

if(FIXED_SEED)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DFIXED_SEED")
endif()

if(NUM_OF_TESTS)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DNUM_OF_TESTS=${NUM_OF_TESTS}")
endif()

if(DEFINED SECURITY_BITS)
  if(SECURITY_BITS STREQUAL "128")
    set(LEVEL 1)
  elseif(SECURITY_BITS STREQUAL "192")
    set(LEVEL 3)
  elseif(SECURITY_BITS STREQUAL "256")
    set(LEVEL 5)
  elseif(SECURITY_BITS STREQUAL "512")
    set(LEVEL 7)
  else()
    message(FATAL_ERROR "Unsupported SECURITY_BITS=${SECURITY_BITS}; use 128, 256, or 512. 192 is kept for compatibility.")
  endif()
elseif(DEFINED LEVEL)
  if(LEVEL STREQUAL "1")
    set(SECURITY_BITS 128)
  elseif(LEVEL STREQUAL "3")
    set(SECURITY_BITS 192)
  elseif(LEVEL STREQUAL "5")
    set(SECURITY_BITS 256)
  elseif(LEVEL STREQUAL "7")
    set(SECURITY_BITS 512)
  else()
    message(FATAL_ERROR "Unsupported internal LEVEL=${LEVEL}; use SECURITY_BITS=128, 256, or 512.")
  endif()
else()
  set(SECURITY_BITS 128)
  set(LEVEL 1)
endif()

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DSECURITY_BITS=${SECURITY_BITS}")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DLEVEL=${LEVEL}")

if(NOT DEFINED BIKE_MLTHRE_ENABLED)
  set(BIKE_MLTHRE_ENABLED 1)
endif()
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DBIKE_MLTHRE_ENABLED=${BIKE_MLTHRE_ENABLED}")

if(NOT DEFINED BIKE_MLTHRE_DELTA_ENABLED)
  set(BIKE_MLTHRE_DELTA_ENABLED 1)
endif()
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DBIKE_MLTHRE_DELTA_ENABLED=${BIKE_MLTHRE_DELTA_ENABLED}")

if(NOT DEFINED BIKE_MLTHRE_SAMPLING)
  set(BIKE_MLTHRE_SAMPLING 0)
endif()
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DBIKE_MLTHRE_SAMPLING=${BIKE_MLTHRE_SAMPLING}")

if(BIKE_MAX_IT_OVERRIDE)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DBIKE_MAX_IT_OVERRIDE=${BIKE_MAX_IT_OVERRIDE}")
endif()

if(BIKE_L7_DYNAMIC_THRESHOLD)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DBIKE_L7_DYNAMIC_THRESHOLD=${BIKE_L7_DYNAMIC_THRESHOLD}")
endif()

if(BIKE_L7_EMPIRICAL_THRESHOLD)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DBIKE_L7_EMPIRICAL_THRESHOLD=${BIKE_L7_EMPIRICAL_THRESHOLD}")
endif()

if(DEFINED BIKE_L7_EMP_TH_A_NUM)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DBIKE_L7_EMP_TH_A_NUM=${BIKE_L7_EMP_TH_A_NUM}")
endif()

if(DEFINED BIKE_L7_EMP_TH_B_NUM)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DBIKE_L7_EMP_TH_B_NUM=${BIKE_L7_EMP_TH_B_NUM}")
endif()

if(DEFINED BIKE_L7_EMP_TH_DEN)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DBIKE_L7_EMP_TH_DEN=${BIKE_L7_EMP_TH_DEN}")
endif()

if(DEFINED BIKE_L7_EMP_TH_MIN)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DBIKE_L7_EMP_TH_MIN=${BIKE_L7_EMP_TH_MIN}")
endif()

if(NOT DEFINED BIKE_L7_REFERENCE_DECODER)
  if(LEVEL STREQUAL "7")
    set(BIKE_L7_REFERENCE_DECODER 1)
  else()
    set(BIKE_L7_REFERENCE_DECODER 0)
  endif()
endif()
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DBIKE_L7_REFERENCE_DECODER=${BIKE_L7_REFERENCE_DECODER}")

if(DEFINED DELTA)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DDELTA=${DELTA}")
endif()

if(UNIFORM_SAMPLING)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DUNIFORM_SAMPLING=1")
endif()

if(BIND_PK_AND_M)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DBIND_PK_AND_M=1")
endif()

# API_PKC evaluation builds use the supplied SM3 hash and pseudoXOF.
if(NOT DEFINED USE_API_PKC_AUX)
  set(USE_API_PKC_AUX ON)
endif()

if(USE_API_PKC_AUX)
  set(USE_SHA3_AND_SHAKE OFF)
  set(STANDALONE_IMPL ON)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DUSE_API_PKC_AUX=1")
else()
  # SHA3 is the default in Round-4 BIKE.
  if(NOT USE_AES_AND_SHA2)
    set(USE_SHA3_AND_SHAKE ON)
  endif()

  # Using SHA3 and SHAKE forces the standalone implementation.
  if(USE_SHA3_AND_SHAKE)
    set(STANDALONE_IMPL ON)
  endif()
endif()

# Standalone implementation features an implementation of AES that uses
# AES-NI and SSE3 x86 instructions. This means that the implementation
# that uses AES based PRF is not fully portable. However, if SHAKE based
# PRF is used (USE_SHA3_AND_SHAKE flag is set) then the implementation
# is fully portable because SHA3 and SHAKE are implemented in pure C.
if(STANDALONE_IMPL)
  if((NOT X86_64) AND (NOT X86) AND (NOT USE_SHA3_AND_SHAKE) AND
     (NOT USE_API_PKC_AUX))
    message(FATAL_ERROR " Standalone implementation with AES based PRNG works only on x86 systems.")
  endif()

  if(USE_SHA3_AND_SHAKE)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DUSE_SHA3_AND_SHAKE=1")
  elseif(NOT USE_API_PKC_AUX)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -maes -mssse3")
  endif()

  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DSTANDALONE_IMPL=1")
else()
  set(LINK_OPENSSL 1)
endif()

if(USE_NIST_RAND)
  if(FIXED_SEED)
    message(FATAL "Can't set both FIXED_SEED and USE_NIST_RAND")
  endif()

  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DUSE_NIST_RAND")

  # This code depends on OpenSSL
  set(LINK_OPENSSL 1)

endif()

# List all files with avx2 and avx512 suffix
FILE(GLOB_RECURSE AVX2_SRCS ${PROJECT_SOURCE_DIR}/src/*_avx2.c)
FILE(GLOB_RECURSE AVX512_SRCS ${PROJECT_SOURCE_DIR}/src/*_avx512.c)

set(AVX512_FLAGS "-mavx512f;-mavx512bw;-mavx512dq")

# Set appropriate flags for avx files
set_source_files_properties(${AVX2_SRCS} PROPERTIES COMPILE_OPTIONS "-mavx2")
set_source_files_properties(${AVX512_SRCS} PROPERTIES COMPILE_OPTIONS "${AVX512_FLAGS}")

set_source_files_properties(${PROJECT_SOURCE_DIR}/src/gf2x/gf2x_mul_base_pclmul.c PROPERTIES COMPILE_OPTIONS "-mpclmul;")
set_source_files_properties(${PROJECT_SOURCE_DIR}/src/gf2x/gf2x_mul_base_vpclmul.c PROPERTIES COMPILE_OPTIONS "-mvpclmulqdq;${AVX512_FLAGS}")
