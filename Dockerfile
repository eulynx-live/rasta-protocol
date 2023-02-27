FROM debian:11

RUN apt-get update && \
    export DEBIAN_FRONTEND=noninteractive && \
    apt-get -y install --no-install-recommends libtool autogen automake git ca-certificates build-essential

ENV LD_LIBRARY_PATH=/usr/local/lib:.

WORKDIR /app
COPY build .
