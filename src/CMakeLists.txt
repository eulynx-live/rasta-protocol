if(ENABLE_RASTA_TLS)
    find_package(WolfSSL REQUIRED)
    include(CheckSymbolExists)
    check_symbol_exists(HAVE_SECRET_CALLBACK "wolfssl/options.h" WOLFSSL_SET_TLS13_SECRET_CB_EXISTS)
    if(WOLFSSL_SET_TLS13_SECRET_CB_EXISTS)
        add_compile_definitions("WOLFSSL_SET_TLS13_SECRET_CB_EXISTS")
        message("Found method wolfSSL_set_tls13_secret_cb - can provide TLS keys for debugging!")
    else()
        message("Could not find method wolfSSL_set_tls13_secret_cb - cannot provide TLS keys for debugging!")
    endif(WOLFSSL_SET_TLS13_SECRET_CB_EXISTS)
endif(ENABLE_RASTA_TLS)

if(ENABLE_RASTA_OPAQUE)
    find_package(sodium REQUIRED)
endif()

# Target name
set(target rasta)

# RaSTA headers
set(headers
    include/rasta/config.h
    include/rasta/event_system.h
    include/rasta/fifo.h
    include/rasta/logging.h
    include/rasta/rasta.h
    include/rasta/rasta_red_multiplexer.h
    include/rasta/rastacrc.h
    include/rasta/rastadeferqueue.h
    include/rasta/rastafactory.h
    include/rasta/rastahandle.h
    include/rasta/rasta_lib.h
    include/rasta/rastamd4.h
    include/rasta/rastamodule.h
    include/rasta/rastaredundancy.h
    include/rasta/rastautil.h
    include/rasta/rmemory.h
    include/rasta/udp.h
    include/rasta/tcp.h
    include/rasta/types.h
    include/rasta/bsd_utils.h
    include/rasta/ssl_utils.h
    include/rasta/rastablake2.h
    include/rasta/rastasiphash24.h
    include/rasta/rastahashing.h
    include/rasta/key_exchange.h
)

set(sources
    c/logging.c
    c/rastafactory.c
    c/transport/rasta_transport_callbacks.h
    c/transport/rasta_transport_callbacks.c
    c/transport/tcp.c
    c/transport/udp.c
    c/transport/bsd_utils.c
    c/retransmission/handlers.c
    c/retransmission/safety_retransmission.h
    c/retransmission/messages.c
    c/retransmission/messages.h
    c/retransmission/safety_retransmission.c
    c/retransmission/protocol.c
    c/retransmission/handlers.h
    c/retransmission/protocol.h
    c/redundancy/rasta_red_multiplexer.c
    c/redundancy/rastaredundancy.c
    c/experimental/handlers.c
    c/experimental/key_exchange.c
    c/experimental/handlers.h
    c/util/rastamodule.c
    c/util/rastacrc.c
    c/util/rastautil.c
    c/util/rastahashing.c
    c/util/rastamd4.c
    c/util/event_system.c
    c/util/rmemory.c
    c/util/fifo.c
    c/util/rastablake2.c
    c/util/rastasiphash24.c
    c/util/rastadeferqueue.c
    c/rasta_lib.c
    c/rasta.c
    c/rastahandle.c
)

if(ENABLE_RASTA_TLS)
    set(RASTA_VARIANTS udp tcp dtls tls)
    set(sources ${sources} c/transport/ssl_utils.c)
else()
    set(RASTA_VARIANTS udp tcp)
endif(ENABLE_RASTA_TLS)

foreach(RASTA_VARIANT ${RASTA_VARIANTS})

    # Shared object for RaSTA protocol
    add_library(${target}_${RASTA_VARIANT} SHARED ${sources} ${headers})

    # Link system libraries for librasta
    target_link_libraries(${target}_${RASTA_VARIANT} pthread)

    # if USE_OPENSSL parameter is passed to cmake -> use openssl md4 implementation
    if(${USE_OPENSSL})
        message("Using OpenSSL MD4 implementation (only standard IV)")

        # define flag to use openssl in rastamd4
        target_compile_definitions(${target}_${RASTA_VARIANT} PUBLIC USE_OPENSSL)
        # link libcrypto
        target_link_libraries(${target}_${RASTA_VARIANT} crypto)
    else()
        message("Using rasta-c MD4 implementation")
    endif()

    #
    # Project options
    #

    set_target_properties(${target}_${RASTA_VARIANT}
        PROPERTIES
        ${DEFAULT_PROJECT_OPTIONS}
    )

    #
    # Include directories
    #

    target_include_directories(${target}_${RASTA_VARIANT}
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include

        INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    )

    #
    # Compile options
    #

    target_compile_options(${target}_${RASTA_VARIANT}
        PRIVATE
        ${DEFAULT_COMPILE_OPTIONS}
    )

    if(ENABLE_RASTA_OPAQUE)
        include(CheckLinkerFlag)
        target_compile_definitions(${target}_${RASTA_VARIANT} PUBLIC ENABLE_OPAQUE)
        # libopaque headers
        target_include_directories(${target}_${RASTA_VARIANT} PUBLIC ${CMAKE_BINARY_DIR}/include)
        target_link_directories(${target}_${RASTA_VARIANT} PUBLIC ${CMAKE_BINARY_DIR}/lib)

        check_linker_flag(C "-static" LINK_STATIC_SUPPORTED)
        check_linker_flag(C "-dynamic" LINK_DYNAMIC_SUPPORTED)
        if(${LINK_DYNAMIC_SUPPORTED} AND ${LINK_STATIC_SUPPORTED})
            message("Linker supports -static")
            # link only the libopaque library statically, and force all modules to be included
            target_link_options(${target}_${RASTA_VARIANT} PUBLIC "LINKER:-static")
            if(APPLE)
                target_link_options(${target}_${RASTA_VARIANT} PUBLIC "LINKER:-force-load LINKER:-lopaque")
                target_link_options(${target}_${RASTA_VARIANT} PUBLIC "LINKER:-dynamic")
            else()
                target_link_options(${target}_${RASTA_VARIANT} PUBLIC LINKER:--whole-archive LINKER:-lopaque LINKER:--no-whole-archive)
                target_link_options(${target}_${RASTA_VARIANT} PUBLIC "LINKER:-Bdynamic")
            endif()
            # consuming application also needs to link libsodium to resolve opaque's dependencies
            target_link_libraries(${target}_${RASTA_VARIANT} sodium)
        else()
            message(FATAL_ERROR "Linker does not support required linker flags!")
        endif()
    endif()

    # Installation for library
    install(TARGETS ${target}_${RASTA_VARIANT}
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
endforeach()

target_compile_definitions(${target}_udp PUBLIC USE_UDP)
target_compile_definitions(${target}_tcp PUBLIC USE_TCP)

if(ENABLE_RASTA_TLS)
    target_compile_definitions(${target}_dtls PUBLIC USE_UDP ENABLE_TLS)
    target_compile_definitions(${target}_tls PUBLIC USE_TCP ENABLE_TLS)

    target_link_libraries(${target}_dtls wolfssl)
    target_link_libraries(${target}_tls wolfssl)
endif(ENABLE_RASTA_TLS)
