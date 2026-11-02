## Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
## SPDX-License-Identifier: Apache-2.0"

FILE(GLOB HEADERS ${CMAKE_CURRENT_LIST_DIR}/*.h)

target_sources(${PROJECT_NAME}
  PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/sampling.c
    ${CMAKE_CURRENT_LIST_DIR}/sampling_portable.c

    ${HEADERS}
)

if(X86_64)
  target_sources(${PROJECT_NAME}
    PRIVATE
      ${CMAKE_CURRENT_LIST_DIR}/sampling_avx2.c
      ${CMAKE_CURRENT_LIST_DIR}/sampling_avx512.c)
endif()

# Select the PRF used by sparse-vector sampling.
if(USE_API_PKC_AUX)
  target_sources(${PROJECT_NAME}
    PRIVATE
      ${CMAKE_CURRENT_LIST_DIR}/aux_prf.c)
elseif(USE_SHA3_AND_SHAKE)
  target_sources(${PROJECT_NAME}
    PRIVATE
      ${CMAKE_CURRENT_LIST_DIR}/shake_prf.c)
else()
  target_sources(${PROJECT_NAME}
    PRIVATE
      ${CMAKE_CURRENT_LIST_DIR}/aes_ctr_prf.c)
endif()

# In standalone implementation we use our own implementations
# of AES and SHA. Note that in case when USE_SHA3_AND_SHAKE is
# defined we don't need aes.c since we are using SHAKE based PRF
# and we don't need sha.c since the SHA-3 implementation is in
# the third_party_src directory.
if(STANDALONE_IMPL AND NOT USE_SHA3_AND_SHAKE AND NOT USE_API_PKC_AUX)
  target_sources(${PROJECT_NAME}
    PRIVATE
      ${CMAKE_CURRENT_LIST_DIR}/sha.c
      ${CMAKE_CURRENT_LIST_DIR}/aes.c)
endif()
