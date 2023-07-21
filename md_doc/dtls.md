# DTLS Support

Optionally, this implementation supports tunneling RaSTA through Datagram TLS (DTLS).
DTLS can be used to improve security against active attackers, at the expense of requiring set-up and maintenance of a Public-Key Infrastructure (PKI).

## Prerequisites
Compiling librasta with DTLS support requires the *wolfssl* run-time libraries and headers. For example, on Debian/Ubuntu, the package *wolfssl-dev* is required. 

## How to enable
In order to enable the support, use the `ENABLE_RASTA_TLS` cmake parameter.
After that, you can use the TLS-related configuration options.
See *examples/config/rasta_[client1|server]_local.cfg* configuration examples for documentation of the options.
In a nutshell, you have to enable DTLS globally and supply the path to the used Root CA certificate (client) or Root CA certificate, server certificate, and server private key (server).
Also, you need to specify the host name of the server certificate.  
The client will validate that the server posesses a certificate that was signed by the given Root CA (and the corresponding private key) and that the hostname matches what was expected, while there is no server-side validation of the client.

## How to test
The *rcat_dtls* binary will generate suitable certificates, start a server and connect a client to the server.
Use the *examples/example_scripts/example_dtls.sh* script to test whether the connection succeeds.