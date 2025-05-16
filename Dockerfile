FROM debian:bookworm-slim AS build
RUN set -e; \
    export DEBIAN_FRONTEND=noninteractive; \
    apt-get update; \
    apt-get -y install cmake libc6-dev libjemalloc-dev g++ make; \
    rm -rf /var/lib/apt/lists/*;
COPY . /usr/local/src/kvmserver/
WORKDIR /build
RUN set -e; \
    cmake /usr/local/src/kvmserver -DCMAKE_BUILD_TYPE=Release; \
    make -j 8;

FROM debian:bookworm-slim
RUN set -e; \
    export DEBIAN_FRONTEND=noninteractive; \
    apt-get update; \
    apt-get -y install libjemalloc2; \
    rm -rf /var/lib/apt/lists/*;
# Convenience for now.
COPY --from=denoland/deno:bin-2.3.1 /deno /usr/local/bin/
COPY --from=build /build/kvmserver /usr/local/bin/


