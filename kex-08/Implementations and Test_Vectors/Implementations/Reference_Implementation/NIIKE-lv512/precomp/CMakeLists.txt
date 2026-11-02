set(SOURCE_FILES_PRECOMP_${SVARIANT_UPPER}
    torsion_constants.c
    protocol_constants.c
    bundleprotocol_constants.c
)

set(INC_PRECOMP_${SVARIANT_UPPER} ${CMAKE_CURRENT_SOURCE_DIR}/include CACHE INTERNAL "INC")
set(LIB_PRECOMP_${SVARIANT_UPPER} niike_precomp_${SVARIANT} CACHE INTERNAL "LIB")
add_library(${LIB_PRECOMP_${SVARIANT_UPPER}} ${SOURCE_FILES_PRECOMP_${SVARIANT_UPPER}})
target_include_directories(${LIB_PRECOMP_${SVARIANT_UPPER}} PRIVATE common ${COMMON_SRC_DIR}/protocols/include ${COMMON_SRC_DIR}/ec/include ${COMMON_SRC_DIR}/gf/include ${INC_PRECOMP_${SVARIANT_UPPER}} ${INC_COMMON_${SVARIANT_UPPER}})
target_compile_options(${LIB_PRECOMP_${SVARIANT_UPPER}} PRIVATE ${C_OPT_FLAGS})