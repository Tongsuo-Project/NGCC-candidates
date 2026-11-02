cmake_minimum_required(VERSION 3.16)
project(TRIKE-5 C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
option(OPTIMIZED_IMPLEMENTATION "Build the optimized AVX-512/VPCLMUL implementation" ON)

file(GLOB TRIKE_SOURCES
	"${CMAKE_CURRENT_SOURCE_DIR}/ICCS/*.c"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
)

add_library(trike5 STATIC ${TRIKE_SOURCES})
target_include_directories(trike5 PUBLIC
	"${CMAKE_CURRENT_SOURCE_DIR}/ICCS"
	"${CMAKE_CURRENT_SOURCE_DIR}/src"
)

if(OPTIMIZED_IMPLEMENTATION)
	if(CMAKE_OSX_ARCHITECTURES)
		if(NOT CMAKE_OSX_ARCHITECTURES STREQUAL "x86_64")
			message(FATAL_ERROR "The optimized implementation requires x86-64 AVX-512/VPCLMUL hardware")
		endif()
	elseif(NOT CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|amd64)$")
		message(FATAL_ERROR "The optimized implementation requires x86-64 AVX-512/VPCLMUL hardware")
	endif()
	if(NOT CMAKE_C_COMPILER_ID MATCHES "^(GNU|Clang|AppleClang)$")
		message(FATAL_ERROR "The optimized implementation requires GCC or Clang")
	endif()
	target_compile_options(trike5 PRIVATE
		-mvpclmulqdq
		-mavx512f
		-mavx512bw
		-mavx512dq
		-mavx512vl
		-mavx512vpopcntdq
	)
	target_compile_definitions(trike5 PRIVATE OPTIMIZED_IMPLEMENTATION)
endif()

add_executable(kat_kem "${CMAKE_CURRENT_SOURCE_DIR}/tests/KAT_KEM.c")
target_link_libraries(kat_kem PRIVATE trike5)
