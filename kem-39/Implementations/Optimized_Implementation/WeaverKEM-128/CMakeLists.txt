include(${CMAKE_CURRENT_SOURCE_DIR}/../cmake/weaver_avx2_iccs.cmake)

weaver_add_optimized_kat(
  KAT_KEM_WeaverKEM-128
  KEM_WeaverKEM-128.c
  1
  ntt3329_avx128.c
  poly_compress_avx.c
  poly_compress9.c
)
