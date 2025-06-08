# syntax=docker/dockerfile:1-labs

# Build on manylinux so binary works on:
# Debian 12+ / Ubuntu 21.10+ / Fedora 35+ / CentOS/RHEL 9+
FROM quay.io/pypa/manylinux_2_34_x86_64 AS build
COPY --exclude=examples . /usr/local/src/kvmserver/
WORKDIR /build
RUN set -e; \
    cmake /usr/local/src/kvmserver -DCMAKE_BUILD_TYPE=Release; \
    make -j 8;

# podman build --target test -t kvmserver-test .
# podman run --rm --device /dev/kvm --group-add keep-groups -it kvmserver-test
FROM ubuntu:24.04 AS test
RUN set -e; \
    export DEBIAN_FRONTEND=noninteractive; \
    apt-get update; \
    apt-get -y install curl g++ gcc libc6-dev make rustup; \
    rm -rf /var/lib/apt/lists/*;
COPY --from=ghcr.io/astral-sh/uv:0.7.11 /uv /uvx /usr/local/bin/
RUN uv python install 3.13.4
RUN CARGO_HOME=/usr rustup toolchain install 1.87
RUN curl -sSL https://github.com/lune-org/lune/releases/download/v0.9.3/lune-0.9.3-linux-x86_64.zip \
    | gunzip -c > /usr/local/bin/lune \
    && chmod +x /usr/local/bin/lune
COPY --from=denoland/deno:bin-2.3.5 /deno /usr/local/bin/
WORKDIR /examples
COPY examples ./
RUN make build
COPY --from=build /build/kvmserver /usr/local/bin/
CMD [ "make", "test", "KVMSERVER=kvmserver" ]

# podman-compose build
# podman-compose up
# podman-compose down --volumes
FROM ubuntu:24.04 AS demo
COPY --from=denoland/deno:bin-2.3.5 /deno /usr/local/bin/
COPY --from=build /build/kvmserver /usr/local/bin/

# podman build --target repro -t kvmserver-repro .
# podman run --rm --device /dev/kvm --group-add keep-groups -it kvmserver-repro
FROM debian:bookworm-slim AS repro
COPY --from=build /build/kvmserver /usr/local/bin/
CMD ["kvmserver", "--allow-all", "--", "bash", "-c", "echo foo"]
