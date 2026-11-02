set(SOURCE_FILES_COMMON_SYS 
    randombytes_system.c 
    aes_c.c 
    fips202.c 
    pike_hash.c
    # mem.c
    tools.c
)

if (PIKE_ENABLE_PROFILE)
    add_library(pike_profile pike_profile.c)
    target_include_directories(pike_profile PUBLIC include ../../include)
    target_compile_options(pike_profile PUBLIC ${C_OPT_FLAGS})
    target_compile_definitions(pike_profile PUBLIC PIKE_PROFILE_ENABLED=1)
endif()

add_library(pike_common_sys ${SOURCE_FILES_COMMON_SYS})
target_include_directories(pike_common_sys PRIVATE include ../../include)
target_compile_options(pike_common_sys PUBLIC ${C_OPT_FLAGS})
if (PIKE_ENABLE_PROFILE)
    target_link_libraries(pike_common_sys PUBLIC pike_profile)
    target_compile_definitions(pike_common_sys PRIVATE PIKE_PROFILE_ENABLED=1)
endif()

set(SOURCE_FILES_COMMON_TEST 
    randombytes_ctrdrbg.c 
    aes_c.c 
    fips202.c 
    pike_hash.c
    # mem.c
    tools.c
)

add_library(pike_common_test ${SOURCE_FILES_COMMON_TEST})
target_include_directories(pike_common_test PRIVATE include ../include)
target_compile_options(pike_common_test PUBLIC ${C_OPT_FLAGS})
if (PIKE_ENABLE_PROFILE)
    target_link_libraries(pike_common_test PUBLIC pike_profile)
    target_compile_definitions(pike_common_test PRIVATE PIKE_PROFILE_ENABLED=1)
endif()

if (ENABLE_CT_TESTING)
    target_compile_definitions(pike_common_sys PUBLIC ENABLE_CT_TESTING)
    target_compile_definitions(pike_common_test PUBLIC ENABLE_CT_TESTING)
endif()
