# syntax=docker/dockerfile:1-labs

# Run with the kvmserver:bin image mounted as /mnt.

# podman build -t kvmserver:test examples
# podman run --rm --mount type=image,src=kvmserver:bin,dst=/mnt --device /dev/kvm --group-add keep-groups -it kvmserver:test
# docker build -t kvmserver:test examples
# KVM_GID=$(getent group kvm | cut -d: -f3) docker run --rm --mount type=image,src=kvmserver:bin,dst=/mnt --device /dev/kvm --group-add $KVM_GID -it kvmserver:test
FROM debian:bookworm-slim
LABEL org.opencontainers.image.description="kvmserver examples test runner"
RUN set -e; \
    export DEBIAN_FRONTEND=noninteractive; \
    apt-get update; \
    apt-get -y install curl g++ gcc libc6-dev make; \
    rm -rf /var/lib/apt/lists/*;
ENV RUSTUP_HOME=/usr/local/share/rustup \
    RUSTUP_AUTO_INSTALL=0 \
    RUSTUP_TOOLCHAIN=1.87.0
RUN curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs \
    | CARGO_HOME=/usr/local sh -s -- -y --no-modify-path --default-toolchain $RUSTUP_TOOLCHAIN
# Binary lune releases require GLIBC_2.38 but bookworm has 2.36
RUN CARGO_HOME=/usr/local cargo install lune@0.9.4 --locked && rm -rf /usr/local/registry
ENV UV_NO_CACHE=true \
    UV_PYTHON_BIN_DIR=/usr/local/bin \
    UV_PYTHON_INSTALL_DIR=/usr/local/share/uv/python
COPY --from=ghcr.io/astral-sh/uv:0.7.11 /uv /uvx /usr/local/bin/
RUN uv python install 3.13.4
COPY --from=denoland/deno:bin-2.3.6 /deno /usr/local/bin/
# Must run as non-root user to avoid Deno.Command setgroups(0, NULL)
RUN useradd --system --home /examples --user-group runner
USER runner
WORKDIR /examples
COPY --chown=runner:runner . .
RUN make build
ENV KVMSERVER=/mnt/kvmserver \
    DENO_NO_UPDATE_CHECK=1
CMD [ "deno", "test", "--allow-all" ]
