# syntax=docker/dockerfile:1

FROM ubuntu:jammy-20230624 AS dependencies-notls
RUN apt-get update && \
    export DEBIAN_FRONTEND=noninteractive && \
    apt-get -y install --no-install-recommends libtool autogen automake autoconf git ca-certificates build-essential cmake pkg-config libcunit1 libcunit1-doc libcunit1-dev
RUN cd /tmp \
 && git clone --recurse-submodules -b v1.47.0 --depth 1 --shallow-submodules https://github.com/grpc/grpc \
 && cd grpc \
 && mkdir -p cmake/build && cd cmake/build \
 && cmake ../.. -DgRPC_INSTALL=ON \
 && make -j16 && make install \
 && cd / && rm -rf /tmp/grpc
ENV LD_LIBRARY_PATH=/usr/local/lib:.

FROM dependencies-notls AS dependencies-tls
RUN cd /tmp \
&& git clone --recurse-submodules -b v5.2.0-stable --depth 1 --shallow-submodules https://github.com/wolfssl/wolfssl \
&& cd wolfssl \
&& ./autogen.sh \
&& ./configure --enable-dtls --enable-debug --enable-certgen --enable-tls13 CFLAGS="-DHAVE_SECRET_CALLBACK" --enable-opensslextra \
&& make && make install \
&& cd / && rm -rf /tmp/wolfssl

FROM dependencies-tls as builder
COPY . /tmp/rasta-protocol/
RUN cd /tmp/rasta-protocol \
 && mkdir -p build \
 && cmake -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_RASTA_GRPC_BRIDGE=ON -DENABLE_RASTA_TLS=ON \
 && cmake --build build --config Release -j16

FROM dependencies-notls as tcp
WORKDIR /app
COPY --from=builder /tmp/rasta-protocol/build/librasta_tcp.so /tmp/rasta-protocol/build/rasta_grpc_bridge_tcp ./
ENTRYPOINT ["/app/rasta_grpc_bridge_tcp"]

FROM dependencies-notls as udp
WORKDIR /app
COPY --from=builder /tmp/rasta-protocol/build/librasta_udp.so /tmp/rasta-protocol/build/rasta_grpc_bridge_udp ./
ENTRYPOINT ["/app/rasta_grpc_bridge_udp"]

FROM dependencies-tls as tls
WORKDIR /app
COPY --from=builder /tmp/rasta-protocol/build/librasta_tls.so /tmp/rasta-protocol/build/rasta_grpc_bridge_tls ./
ENTRYPOINT ["/app/rasta_grpc_bridge_tls"]

FROM dependencies-tls as dtls
WORKDIR /app
COPY --from=builder /tmp/rasta-protocol/build/librasta_dtls.so /tmp/rasta-protocol/build/rasta_grpc_bridge_dtls ./
ENTRYPOINT ["/app/rasta_grpc_bridge_dtls"]
