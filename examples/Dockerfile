# syntax=docker/dockerfile:1-labs

# Run with the kvmserver:bin image (or .build directory) mounted as /mnt.

# podman build -t kvmserver:test examples
# podman run --rm --mount type=image,src=kvmserver:bin,dst=/mnt --device /dev/kvm --group-add keep-groups -it kvmserver:test
# docker build -t kvmserver:test examples
# KVM_GID=$(getent group kvm | cut -d: -f3) docker run --rm --mount type=image,src=kvmserver:bin,dst=/mnt --device /dev/kvm --group-add $KVM_GID -it kvmserver:test
FROM debian:bookworm-slim
LABEL org.opencontainers.image.source=https://github.com/libriscv/kvmserver
LABEL org.opencontainers.image.description="kvmserver examples test runner"
RUN set -e; \
    export DEBIAN_FRONTEND=noninteractive; \
    apt-get update; \
    apt-get -y install curl g++ gcc libc6-dev make; \
    rm -rf /var/lib/apt/lists/*;
ENV CARGO_HOME=/usr/local
RUN curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs \
    | sh -s -- -y --no-modify-path --default-toolchain 1.87.0
# Newer lune releases require GLIBC_2.38 but bookworm has 2.36
RUN curl -sSL https://github.com/lune-org/lune/releases/download/v0.8.9/lune-0.8.9-linux-x86_64.zip \
    | gunzip -c > /usr/local/bin/lune \
    && chmod +x /usr/local/bin/lune
ENV UV_NO_CACHE=true \
    UV_PYTHON_BIN_DIR=/usr/local/bin \
    UV_PYTHON_INSTALL_DIR=/usr/local/share/uv/python
COPY --from=ghcr.io/astral-sh/uv:0.7.11 /uv /uvx /usr/local/bin/
RUN uv python install 3.13.4
COPY --from=denoland/deno:bin-2.3.5 /deno /usr/local/bin/
WORKDIR /examples
COPY . .
ENV DENO_DIR=/examples/.cache/deno
RUN make build
# Must run as non-root user to avoid 
USER nobody
ENV KVMSERVER=/mnt/kvmserver
CMD [ "deno", "test", "--allow-all" ]
