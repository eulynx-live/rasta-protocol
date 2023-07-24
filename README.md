# RaSTA

C library implementation of the Rail Safe Transport Application (RaSTA) protocol.

This implementation of the RaSTA protocol supports Datagram TLS (DTLS), see [TLS HowTo](md_doc/dtls.md). 

The project is built with CMake. 
It provides different variants of `librasta` as an output, depending on which transport layer is required:

- librasta_udp
- librasta_tcp
- librasta_dtls (if `ENABLE_RASTA_TLS` is enabled)
- librasta_tls (if `ENABLE_RASTA_TLS` is enabled)

## Deployment

### Unit tests

see [CUnit HowTo](md_doc/cunit.md)  

### How to use the RaSTA library

see [Getting started](md_doc/getting_started.md)

## Built With

* [CUnit](http://cunit.sourceforge.net/) - For Unit tests
* [CppCheck](http://cppcheck.sourceforge.net/) - For static code analysis
* [Doxygen](http://www.stack.nl/~dimitri/doxygen/) - Documentation generation
* [CMake](https://cmake.org/)  - Compilation


## Extensions 

Orthogonally to DTLS/TLS, this implementation of the RaSTA protocol also supports an additional key exchange phase after the initial handshake.
During this phase, using the OPAQUE password authenticated key exchange protocol and a user-supplied pre-shared key, a session key with high randomness is exchanged between the peers.
Also, it is verified that both peers have knowledge of the PSK.
For more details, see [OPAQUE HowTo](md_doc/opaque.md).
