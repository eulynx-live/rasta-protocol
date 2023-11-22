if(USE_VCPKG)
  find_package(unofficial-cunit CONFIG REQUIRED)
  set(CUNIT_PACKAGE unofficial::cunit::cunit)
else()
  include(FindPkgConfig)
  if(PKG_CONFIG_FOUND)
    pkg_check_modules(CUnit IMPORTED_TARGET cunit)
    set(CUNIT_PACKAGE PkgConfig::CUnit)
  endif(PKG_CONFIG_FOUND)
endif(USE_VCPKG)

set(EXAMPLES_COMMON_SRC
    common/c/configfile.c
    common/c/dictionary.c)

if(BUILD_LOCAL_EXAMPLES)
add_executable(rcat_udp
                ${EXAMPLES_COMMON_SRC}
                rcat/c/rcat.c)
target_include_directories(rcat_udp PRIVATE common/headers)
set_target_properties(rcat_udp PROPERTIES ${DEFAULT_PROJECT_OPTIONS})
target_compile_options(rcat_udp PRIVATE ${DEFAULT_COMPILE_OPTIONS})
target_link_libraries(rcat_udp rasta_udp)

add_executable(rcat_tcp
                ${EXAMPLES_COMMON_SRC}
                rcat/c/rcat.c)
target_include_directories(rcat_tcp PRIVATE common/headers)
set_target_properties(rcat_tcp PROPERTIES ${DEFAULT_PROJECT_OPTIONS})
target_compile_options(rcat_tcp PRIVATE ${DEFAULT_COMPILE_OPTIONS})
target_link_libraries(rcat_tcp rasta_tcp)


add_executable(event_system_example_local
                ${EXAMPLES_COMMON_SRC}
                examples_localhost/c/event_test.c)
target_include_directories(event_system_example_local PRIVATE common/headers)
set_target_properties(event_system_example_local PROPERTIES ${DEFAULT_PROJECT_OPTIONS})
target_compile_options(event_system_example_local PRIVATE ${DEFAULT_COMPILE_OPTIONS})
target_link_libraries(event_system_example_local rasta_udp)

add_executable(pw2iv_sample
        examples_localhost/c/pw2iv.c)
target_compile_options(pw2iv_sample PRIVATE ${DEFAULT_COMPILE_OPTIONS})

if(ENABLE_RASTA_TLS)
        add_executable(rcat_dtls
                ${EXAMPLES_COMMON_SRC}
                rcat/c/dtls.c rcat/c/wolfssl_certificate_helper.c rcat/c/wolfssl_certificate_helper.h)
        target_include_directories(rcat_dtls PRIVATE common/headers)
        set_target_properties(rcat_dtls PROPERTIES ${DEFAULT_PROJECT_OPTIONS})
        target_compile_options(rcat_dtls PRIVATE ${DEFAULT_COMPILE_OPTIONS})
        target_link_libraries(rcat_dtls rasta_dtls wolfssl)
        add_executable(rcat_tls
                ${EXAMPLES_COMMON_SRC}
                rcat/c/tls.c rcat/c/wolfssl_certificate_helper.c rcat/c/wolfssl_certificate_helper.h)
        target_include_directories(rcat_tls PRIVATE common/headers)
        set_target_properties(rcat_tls PROPERTIES ${DEFAULT_PROJECT_OPTIONS})
        target_compile_options(rcat_tls PRIVATE ${DEFAULT_COMPILE_OPTIONS})
        target_link_libraries(rcat_tls rasta_tls wolfssl)
endif()

if(ENABLE_RASTA_OPAQUE)
        add_executable(rcat_kex ${EXAMPLES_COMMON_SRC} rcat/c/kex.c)
        target_include_directories(rcat_kex PRIVATE common/headers)
        set_target_properties(rcat_kex PROPERTIES ${DEFAULT_PROJECT_OPTIONS})
        target_compile_options(rcat_kex PRIVATE ${DEFAULT_COMPILE_OPTIONS})
        target_link_libraries(rcat_kex rasta_udp sodium)

        add_executable(record_generator ${EXAMPLES_COMMON_SRC} rcat/c/record_generator.c)
        target_include_directories(record_generator PRIVATE common/headers)
        set_target_properties(record_generator PROPERTIES ${DEFAULT_PROJECT_OPTIONS})
        target_compile_options(record_generator PRIVATE ${DEFAULT_COMPILE_OPTIONS})
        target_link_libraries(record_generator rasta_udp sodium)
endif()

# Copy RaSTA configs to build directory
configure_file(config/rasta_server_local.cfg rasta_server_local.cfg COPYONLY)
configure_file(config/rasta_client_local.cfg rasta_client_local.cfg COPYONLY)

configure_file(config/rasta_server_local_dtls.cfg rasta_server_local_dtls.cfg COPYONLY)
configure_file(config/rasta_client_local_dtls.cfg rasta_client_local_dtls.cfg COPYONLY)

configure_file(config/rasta_server_local_tls.cfg rasta_server_local_tls.cfg COPYONLY)
configure_file(config/rasta_client_local_tls.cfg rasta_client_local_tls.cfg COPYONLY)

configure_file(config/rasta_server_local_kex.cfg rasta_server_local_kex.cfg COPYONLY)
configure_file(config/rasta_client1_local_kex.cfg rasta_client1_local_kex.cfg COPYONLY)
configure_file(config/rasta_client2_local_kex.cfg rasta_client2_local_kex.cfg COPYONLY)
endif(BUILD_LOCAL_EXAMPLES)

if(BUILD_RASTA_GRPC_BRIDGE)
add_subdirectory(rasta_grpc_bridge)
endif(BUILD_RASTA_GRPC_BRIDGE)

set(SCI_COMMON
    ${EXAMPLES_COMMON_SRC}
    sci/common/hashmap.h
    sci/common/hashmap.c
    sci/common/sci_telegram_factory.c
    sci/common/sci_telegram_factory.h
    sci/common/sci.c
    sci/common/sci.h
    sci/common/scils_telegram_factory.c
    sci/common/scils_telegram_factory.h
    sci/common/scils.c
    sci/common/scils.h
    sci/common/scip_telegram_factory.c
    sci/common/scip_telegram_factory.h
    sci/common/scip.c
    sci/common/scip.h
)

# scip_example
add_executable(scip_example ${SCI_COMMON} sci/scip/main.c)
target_include_directories(scip_example PRIVATE sci/common common/headers)
set_target_properties(scip_example PROPERTIES ${DEFAULT_PROJECT_OPTIONS})
target_compile_options(scip_example PRIVATE ${DEFAULT_COMPILE_OPTIONS})
target_link_libraries(scip_example rasta_udp)

# scils_example
add_executable(scils_example ${SCI_COMMON} sci/scils/main.c)
target_include_directories(scils_example PRIVATE sci/common common/headers)
set_target_properties(scils_example PROPERTIES ${DEFAULT_PROJECT_OPTIONS})
target_compile_options(scils_example PRIVATE ${DEFAULT_COMPILE_OPTIONS})
target_link_libraries(scils_example rasta_udp)

add_executable(sciTest
    ${SCI_COMMON}
    sci/scitest/headers/registerTests.h
    sci/scitest/headers/scilsTests.h
    sci/scitest/headers/scipTests.h
    sci/scitest/headers/sciTests.h
    sci/scitest/c/registerTests.c
    sci/scitest/c/scilsTests.c
    sci/scitest/c/scipTests.c
    sci/scitest/c/sciTests.c)
target_include_directories(sciTest PRIVATE sci/common sci/scitest/headers common/headers)
target_link_libraries(sciTest rasta_udp ${CUNIT_PACKAGE})
target_compile_definitions(sciTest PRIVATE WITH_CMAKE)
add_test(NAME test_sciTest COMMAND sciTest)
