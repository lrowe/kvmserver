# syntax=docker/dockerfile:1-labs

# Run with the kvmserver:bin image mounted as /mnt.

# podman build -t ghcr.io/libriscv/kvmserver:test examples
# podman run --rm --mount type=image,src=ghcr.io/libriscv/kvmserver:bin,dst=/mnt --device /dev/kvm --group-add keep-groups -it ghcr.io/libriscv/kvmserver:test
# docker build -t ghcr.io/libriscv/kvmserver:test examples
# KVM_GID=$(getent group kvm | cut -d: -f3) docker run --rm --mount type=image,src=ghcr.io/libriscv/kvmserver:bin,dst=/mnt --device /dev/kvm --group-add $KVM_GID -it ghcr.io/libriscv/kvmserver:test

FROM node:24.6.0-trixie-slim
LABEL org.opencontainers.image.description="kvmserver examples test runner"
RUN set -e; \
    export DEBIAN_FRONTEND=noninteractive; \
    apt-get update; \
    apt-get -y install curl g++ gcc libc6-dev make rustup xz-utils; \
    rm -rf /var/lib/apt/lists/*;
ENV RUSTUP_HOME=/usr/local/share/rustup
ENV RUSTUP_AUTO_INSTALL=0
ENV RUSTUP_TOOLCHAIN=1.89.0
RUN rustup install $RUSTUP_TOOLCHAIN
RUN rustup target add wasm32-wasip2
RUN curl -fsSL https://github.com/lune-org/lune/releases/download/v0.10.1/lune-0.10.1-linux-x86_64.zip \
    | gunzip -c > /usr/local/bin/lune \
    && chmod +x /usr/local/bin/lune
ENV UV_NO_CACHE=true \
    UV_PYTHON_BIN_DIR=/usr/local/bin \
    UV_PYTHON_INSTALL_DIR=/usr/local/share/uv/python
COPY --from=ghcr.io/astral-sh/uv:0.8.13 /uv /uvx /usr/local/bin/
RUN uv python install 3.13.7
RUN curl -fsSL https://github.com/bytecodealliance/wasmtime/releases/download/v36.0.2/wasmtime-v36.0.2-x86_64-linux.tar.xz \
    | tar -xJC /usr/local/bin/ --strip-components=1 --wildcards "*/wasmtime"
COPY --from=ghcr.io/lrowe/wasmtime:bin-git.bce7f2054f2b2d84d8adfa2c6ce813863721f2ea /wasmtime /usr/local/bin/wasmtime-noreuse
COPY --from=ghcr.io/lrowe/wasmtime:bin-git.e985d36161da43cfb9a8d7c581c6a7a91378a57e /wasmtime /usr/local/bin/wasmtime-reuse
ADD --chmod=755 https://github.com/hatoo/oha/releases/download/v1.9.0/oha-linux-amd64-pgo /usr/local/bin/oha
COPY --from=denoland/deno:bin-2.5.0 /deno /usr/local/bin/
COPY --from=oven/bun:1.2.21-debian /usr/local/bin/bun /usr/local/bin/
# Must run as non-root user to avoid Deno.Command setgroups(0, NULL) https://github.com/denoland/deno/issues/29770
RUN useradd --system --home /examples --user-group runner
RUN set -e; \
    mkdir -p /src/api; \
    cd src/api; \
    curl -fsSLO https://raw.githubusercontent.com/libriscv/kvmserver/29ca1a783b57f5723d600339dfa363261551a2d3/src/api/libkvmserverguest.c
USER runner
WORKDIR /examples
COPY --chown=runner:runner . .
RUN make build
ENV KVMSERVER=/mnt/kvmserver \
    DENO_NO_UPDATE_CHECK=1
CMD [ "deno", "test", "--allow-all" ]
