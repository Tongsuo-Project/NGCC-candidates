set(SOURCE_FILES_HD_GENERIC_REF
    ${HDX_DIR}/hd.c
    ${HDX_DIR}/theta_structure.c 
    ${HDX_DIR}/theta_isogenies.c 
)

add_library(${LIB_HD_${SVARIANT_UPPER}} ${SOURCE_FILES_HD_GENERIC_REF})
target_include_directories(${LIB_HD_${SVARIANT_UPPER}} PRIVATE ${INC_INTBIG} ${INC_PRECOMP_${SVARIANT_UPPER}} ${INC_PUBLIC} ${INC_GF} ${INC_GF_${SVARIANT_UPPER}} ${INC_COMMON} ${INC_EC} ${INC_HD})
target_link_libraries(${LIB_HD_${SVARIANT_UPPER}} ${LIB_INTBIG_${SVARIANT_UPPER}} ${LIB_EC_${SVARIANT_UPPER}})
target_compile_options(${LIB_HD_${SVARIANT_UPPER}} PRIVATE ${C_OPT_FLAGS})
if (PIKE_ENABLE_NGCC1_PROFILE)
    target_link_libraries(${LIB_HD_${SVARIANT_UPPER}} pike_profile)
    target_compile_definitions(${LIB_HD_${SVARIANT_UPPER}} PRIVATE PIKE_PROFILE_ENABLED=1)
endif()

add_subdirectory(test)
