## Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
## SPDX-License-Identifier: Apache-2.0"

FILE(GLOB HEADERS ${CMAKE_CURRENT_LIST_DIR}/*.h)

if(USE_API_PKC_AUX)
  # The supplied file must remain byte-for-byte unchanged. Its parameter names
  # trigger this project's global -Wshadow setting.
  set_source_files_properties(
    ${CMAKE_CURRENT_LIST_DIR}/auxfunc.c
    PROPERTIES COMPILE_OPTIONS "-Wno-shadow")
  target_sources(${PROJECT_NAME}
    PRIVATE
      ${CMAKE_CURRENT_LIST_DIR}/auxfunc.c
      ${CMAKE_CURRENT_LIST_DIR}/auxfunc.h)
elseif(USE_SHA3_AND_SHAKE)
  target_sources(${PROJECT_NAME}
    PRIVATE
      ${CMAKE_CURRENT_LIST_DIR}/fips202.c)
endif()
