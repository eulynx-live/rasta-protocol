# RaSTA - Getting started
In this tutorial, you will learn how to deploy and use the RaSTA C library.
This tutorial is written for Linux. If you are using another operating system, the commands should be similar.

## 1. Building the library

### Dependencies

The following dependencies are required for building the RaSTA library and binaries:

* CUnit (package `libcunit1` or similar)
* if `ENABLE_RASTA_OPAQUE` is enabled: libsodium (packages `libsodium-dev` and `pkgconf` or similar)
* if `ENABLE_RASTA_TLS` is enabled: [WolfSSL](https://www.wolfssl.com/) (package `wolfssl` or similar). built manually with the configuration flags `--enable-dtls --enable-debug --enable-certgen --enable-tls13 CFLAGS="-DHAVE_SECRET_CALLBACK" --enable-opensslextra`
* if `BUILD_RASTA_GRPC_BRIDGE` is enabled: gRPC (package `grpc` or similar)

We tested with WolfSSL version 5.2.0-stable and gRPC version 1.47.0. For other versions, we cannot guarantee that you will be able to build the library.

For debug purposes (if you want to dissect recorded TLS traffic with Wireshark), it can be useful to enable the *secret callback* of WolfSSL, which gives you access to the TLS connection secrets. For this, WolfSSL needs to be built manually with the configuration flags `--enable-dtls --enable-debug --enable-certgen --enable-tls13 CFLAGS="-DHAVE_SECRET_CALLBACK" --enable-opensslextra`.

### Build options

We provide a number of build options, of which the following are the most important:

* `BUILD_DOCUMENTATION`: use Doxygen to build an HTML documentation for the library
* `BUILD_LOCAL_EXAMPLES`: build the examples described in part 2
* `BUILD_RASTA_GRPC_BRIDGE`: build the RaSTA/gRPC bridge described in part 2
* `ENABLE_RASTA_TLS`: include the TLS and DTLS transport implementations in the library that will be built
* `ENABLE_RASTA_OPAQUE`: include the Kex/OPAQUE implementation in the library that will be built

### Build process

For the actual build process, we use CMake (>= 3.18). With the following commands, you can build the library and binaries:

```
mkdir -p build
cd build
cmake ..
cmake --build .
```

You will get one library file for each transport implementation you selected (TCP/UDP/TLS/DTLS), named `librasta_{protocol}.so`.
To install the library files on the system, you may use `make install`, if needed (might need root privileges).

Note that on ARM systems, our default MD4 implementation does not work correctly, so you should use OpenSSL/`libcrypto` as a replacement. In this case, enable `USE_OPENSSL`.

## 2. Using the examples
The RaSTA C library comes with some simple examples to show the use of the library.
The following examples are included:

- **mux_stresstest:** a simple forwarding example using only the redundancy layer. A client sends a message to a server which forwards the received message to another client.
- **rcat:** an example for communication between a client and a server (provided in versions for all supported transport protocols), which allows sending text submitted on the commandline between client and server. Use commandline argument `r` to start in server (receiver) mode and `s` to start in client (sender) mode. Note that these examples should be run from a folder containing the config files `rasta_server_local{_tls,_dtls}.cfg` and `rasta_client_local{_tls,_dtls}.cfg`.
- **scils_example** and **scip_example**: examples for the communication with points and light signals using SCI-P and SCI-LS. These examples just send one simple SCI telegram to change the point position / signal aspect and wait for the corresponding status message.
- **rasta_grpc_bridge**: an extremely useful program, which sends messages submitted via gRPC on a RaSTA connection and sends received RaSTA messages back to you, also via gRPC. This allows you to fully focus on your application specific protocol without needing to know RaSTA.
- **examples_localhost** and **logging_example**: These examples show you (as a RaSTA library developer) how logging, events and MD4 work. They are also meant to test these specific modules.

### The Rasta/gRPC bridge

This bridge forwards messages between a RaSTA connection and a gRPC connection. It takes the following commandline arguments:

1. The path of the RaSTA configuration file
2. If the bridge is started as a gRPC server, the address (IP:port) to listen on. Otherwise, this is ignored.
3. The IP address of the first transport channel of the remote RaSTA endpoint.
4. The port of the first transport channel of the remote RaSTA endpoint.
5. The IP address of the second transport channel of the remote RaSTA endpoint.
6. The port of the second transport channel of the remote RaSTA endpoint.
7. The RaSTA ID of the bridge. If this is larger than the ID of the remote endpoint, the bridge becomes a RaSTA server.
8. The RaSTA ID of the remote connection endpoint.
9. If the bridge is started as a gRPC client, the address (IP:port) of the server to connect to. Otherwise, this argument must not be present.

When implementing a server or client able to communicate with the bridge, you must implement (or call) the RPC `Stream()` defined in `rasta.proto`, which takes a stream of bytes and returns a stream of bytes, both corresponding to the data sent on the RaSTA connection. Note that you must not end this stream while your program runs and wants to send/receive data on this connection, because otherwise, the RaSTA connection will be disconnected and currently cannot be recreated without restarting the bridge.

We provide Dockerfiles to create images containing the gRPC bridge and all dependencies (see the `docker/rasta_grpc_bridge` folder). There are also prebuilt images available, which can be found in the "Packages" section of the GitHub repository.

## 3. Using the library
You learned to compile the sources and run the example programs, now it's time to write your own program!
In this section the various functions and some other things are listed and explained.

### Functions
The complete functionality of the library can be used by including `rasta.h`.

| Name                                       | Description                                                                                                                                      |
| ------------------------------------------ | ------------------------------------------------------------------------------------------------------------------------------------------------ |
| `rasta_lib_init_configuration`             | initializes a provide `user_configuration` with a RaSTA configuration, logger and connection configurations.                                     |
| `rasta_bind`/`rasta_listen`/`rasta_accept` | on the server side, these functions bind to, listen on and accept connections on the configured sockets.                                         |
| `rasta_connect`                            | on the client side, this connects to another RaSTA entity. You have to pass the ID of the remote entity as parameter.                            |
| `rasta_send`                               | sends a message to a connected entity (connect with `rasta_connect`) on the passed connection.                                                   |
| `rasta_recv`                               | gets the first message (i.e. the application message that arrived first in regard to time and order in the RaSTA PDU) from the receive buffer of the passed connection. If the buffer is empty, this call will block until an application message is available.                                                                                      |
| `rasta_disconnect`                         | sends a disconnection request to the passed RaSTA connection and closes this connection.                                                         |
| `rasta_cleanup`                            | cleans up allocated ressources etc. Call this at the end of you program to avoid memory leak and some other problems (see *Further Information*) |

### Notifications
The notifications are an easy way to react to events that occur during the protocol flow. Notifications are basically function pointers which you can set. The functions will be called when the respective event occurs. The notification functions have to be assigned in an initialized handle (`handle.notifications`).

**Note: Notifications are currently not working (i.e., commented out in the code)!**

This is a list of all available notifications.

| Name                                    | Event                                                                                                               |
| --------------------------------------- | ------------------------------------------------------------------------------------------------------------------- |
| `on_receive`                            | an application message has been received an is available in the buffer                                              |
| `on_disconnection_request_received`     | a disconnection request has been received and the connection is closed. You can access the reason and details here! |
| `on_connection_state_change`            | the state of the connection has changed                                                                             |
| `on_diagnostic_notification`            | diagnose data of the send-/retransmission layer is available                                                        |
| `on_redundancy_diagnostic_notification` | diagnose data of the redundancy layer is available                                                                  |

### Configuration
In general, the configuration can be specified in a configuration file. In the configuration file, the RaSTA protocol parameters as well as some miscellaneous options like logging can be configured. Every option is documented in the example config files and their meaning should be easily understandable. The only one that is a bit more tricky is `RASTA_REDUNDANCY_CONNECTIONS`.
This option is used to specify the network interfaces and ports where the RaSTA entity will listen on. The format is an array of strings with format `IP:Port` where the IP corresponds to the IP address a network interface is bound to. If you, for whatever reason, want to listen on any interface, use `0.0.0.0` as the IP.
Note that the send-behaviour in this case might not work as you expect (which interface sends the PDUs)!

## 4. Further Information

### Network interface IP by interface name
If you want to get a network interfaces associated IP address by its name (e.g. `eth0`), for example because the IP is assigned dynamically with DHCP, have a look at the system function `getifaddrs` from `ifaddrs.h`. See the [Manpage](http://man7.org/linux/man-pages/man3/getifaddrs.3.html)  for more information.
However, you can't use the configuration file in this case. Use the manual configuration instead.

