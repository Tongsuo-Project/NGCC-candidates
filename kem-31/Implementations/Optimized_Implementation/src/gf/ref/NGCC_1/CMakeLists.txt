set(SOURCE_FILES_GF_${SVARIANT_UPPER}_REF
    ${GFX_DIR}/fp2.c
    ${GFX_DIR}/mp.c
    fp_p479.c
    fp_p479_asm.S
)
set_source_files_properties(${GFX_DIR}/fp2.c PROPERTIES COMPILE_OPTIONS "-mtune=native;-fno-stack-protector;-fcf-protection=none")
set_source_files_properties(fp_p479.c PROPERTIES COMPILE_OPTIONS "-mtune=native;-fno-stack-protector;-fcf-protection=none")

add_library(${LIB_GF_${SVARIANT_UPPER}} ${SOURCE_FILES_GF_${SVARIANT_UPPER}_REF})
target_include_directories(${LIB_GF_${SVARIANT_UPPER}} PRIVATE ${INC_COMMON} ${INC_PRECOMP_${SVARIANT_UPPER}} ${INC_GF} include ${PROJECT_SOURCE_DIR}/include)
target_compile_options(${LIB_GF_${SVARIANT_UPPER}} PRIVATE ${C_OPT_FLAGS})
if (PIKE_ENABLE_NGCC1_PROFILE)
    target_link_libraries(${LIB_GF_${SVARIANT_UPPER}} PUBLIC pike_profile)
    target_compile_definitions(${LIB_GF_${SVARIANT_UPPER}} PRIVATE PIKE_PROFILE_ENABLED=1)
endif()

add_subdirectory(test)
