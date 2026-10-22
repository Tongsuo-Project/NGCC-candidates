cmake_minimum_required(VERSION 3.12)
project(NIIKE VERSION 1.0 LANGUAGES C ASM)

set(CMAKE_C_STANDARD 99)

set(C_OPT_FLAGS "")
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    add_definitions(-DRADIX_64)
else()
    message(FATAL_ERROR "This codebase currently supports only 64-bit systems")
endif()

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-error=implicit-function-declaration -Wno-error=incompatible-pointer-types")
endif()
if(CMAKE_C_COMPILER_ID MATCHES "Clang")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-error=macro-redefined")
endif()

add_subdirectory(NIIKE-lv128)
add_subdirectory(NIIKE-lv256)
add_subdirectory(NIIKE-lv512)