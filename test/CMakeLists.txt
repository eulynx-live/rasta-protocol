
include(FindPkgConfig)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(CUnit IMPORTED_TARGET cunit)
endif(PKG_CONFIG_FOUND)

add_executable(rasta_test
    ../examples/common/c/configfile.c
    ../examples/common/c/dictionary.c
    rasta_test/headers/blake2_test.h
    rasta_test/headers/config_test.h
    rasta_test/headers/dictionary_test.h
    rasta_test/headers/fifo_test.h
    rasta_test/headers/rastacrc_test.h
    rasta_test/headers/rastadeferqueue_test.h
    rasta_test/headers/rastafactory_test.h
    rasta_test/headers/rastamd4_test.h
    rasta_test/headers/rastamodule_test.h
    rasta_test/headers/register_tests.h
    rasta_test/headers/siphash24_test.h
    rasta_test/headers/opaque_test.h
    rasta_test/headers/safety_retransmission_test.h
    rasta_test/c/blake2_test.c
    rasta_test/c/config_test.c
    rasta_test/c/dictionary_test.c
    rasta_test/c/fifo_test.c
    rasta_test/c/rastacrc_test.c
    rasta_test/c/rastadeferqueue_test.c
    rasta_test/c/rastafactory_test.c
    rasta_test/c/rastamd4_test.c
    rasta_test/c/rastamodule_test.c
    rasta_test/c/register_tests.c
    rasta_test/c/siphash24_test.c
    rasta_test/c/opaque_test.c
    rasta_test/c/safety_retransmission_test.c
)
target_include_directories(rasta_test PRIVATE rasta_test/headers ../examples/common/headers)
target_link_libraries(rasta_test rasta_udp PkgConfig::CUnit)
target_compile_definitions(rasta_test PRIVATE WITH_CMAKE)

add_dependencies(rasta_test rasta_udp)

add_test(NAME test_rasta_test
         COMMAND rasta_test)
