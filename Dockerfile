# syntax=docker/dockerfile:1-labs

# Build on manylinux so binary works on:
# Debian 12+ / Ubuntu 21.10+ / Fedora 35+ / CentOS/RHEL 9+
FROM quay.io/pypa/manylinux_2_34_x86_64 AS build
COPY . /usr/local/src/kvmserver/
WORKDIR /build
RUN set -e; \
    cmake /usr/local/src/kvmserver -DCMAKE_BUILD_TYPE=Release; \
    make -j 8;

# podman build --target bin -t ghcr.io/libriscv/kvmserver:bin .
# docker build --target bin -t ghcr.io/libriscv/kvmserver:bin .
FROM scratch AS bin
LABEL org.opencontainers.image.description="kvmserver only the binary"
LABEL org.opencontainers.image.licenses=GPL-3.0-or-later
COPY --from=build /build/kvmserver /kvmserver
