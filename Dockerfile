# Build on manylinux so binary works on:
# Debian 12+ / Ubuntu 21.10+ / Fedora 35+ / CentOS/RHEL 9+
FROM quay.io/pypa/manylinux_2_34_x86_64 AS build
COPY . /usr/local/src/kvmserver/
WORKDIR /build
RUN set -e; \
    cmake /usr/local/src/kvmserver -DCMAKE_BUILD_TYPE=Release; \
    make -j 8;

FROM debian:bookworm-slim
# Convenience for now.
COPY --from=denoland/deno:bin-2.3.1 /deno /usr/local/bin/
COPY --from=build /build/kvmserver /usr/local/bin/
