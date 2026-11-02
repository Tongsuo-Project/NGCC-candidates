cmake_minimum_required(VERSION 3.0.0)
project(bike C)

set(INCLUDE_DIR ${PROJECT_SOURCE_DIR}/include)
set(SRC_DIR ${PROJECT_SOURCE_DIR}/src)
set(TESTS_DIR ${PROJECT_SOURCE_DIR}/tests)
set(KAT_DIR ${PROJECT_SOURCE_DIR}/kat)

include_directories(${INCLUDE_DIR})
include_directories(${INCLUDE_DIR}/internal)
include_directories(${SRC_DIR}/third_party_src)

include(cmake/arch.cmake)
include(cmake/compilation-flags.cmake)
include(cmake/clang-format.cmake)

add_library(${PROJECT_NAME} "")
add_executable(bike-test "")
add_executable(api-pkc-kat "")

target_link_libraries(${PROJECT_NAME} PUBLIC m)
target_include_directories(api-pkc-kat PRIVATE ${KAT_DIR} ${TESTS_DIR}/FromNIST)

add_subdirectory(${SRC_DIR})
add_subdirectory(${TESTS_DIR})

target_link_libraries(bike-test ${PROJECT_NAME})
target_link_libraries(api-pkc-kat ${PROJECT_NAME})

target_sources(api-pkc-kat
  PRIVATE
    ${KAT_DIR}/KAT_KEM.c
    ${KAT_DIR}/KEM_AlgorithmInstance.c
    ${KAT_DIR}/drng.c
    ${KAT_DIR}/KEM_AlgorithmInstance.h
    ${KAT_DIR}/drng.h
    ${TESTS_DIR}/FromNIST/rng.c
)

if(LINK_OPENSSL)
  message(STATUS "Linking OpenSSL")
  find_package(OpenSSL REQUIRED)
  set(OPENSSL_USE_STATIC_LIBS TRUE)
  target_link_libraries(bike-test OpenSSL::Crypto)
  target_link_libraries(api-pkc-kat OpenSSL::Crypto)
endif()
