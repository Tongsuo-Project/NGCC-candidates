include(${CMAKE_CURRENT_SOURCE_DIR}/../cmake/weaver_avx2_iccs.cmake)

set(_avx256_src "")
if(WEAVER_USE_AVX_NTT7681)
  list(APPEND _avx256_src ntt7681_avx.c)
endif()
if(WEAVER_USE_AVX_COMPRESS7681)
  list(APPEND _avx256_src poly_compress_avx.c)
endif()

weaver_add_optimized_kat(
  KAT_KEM_WeaverKEM-256
  KEM_WeaverKEM-256.c
  3
  ${_avx256_src}
)
