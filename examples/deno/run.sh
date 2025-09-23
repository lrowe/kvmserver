set -e
pushd ../../.build
make -j8
popd

export DENO_V8_FLAGS=--predictable,--max-old-space-size=64,--max-semi-space-size=64
export DENO_NO_UPDATE_CHECK=1
rm -f deno.sock
../../.build/kvmserver -e --warmup 1000 --storage --storage-1-to-1 --allow-all ~/.deno/bin/deno -- run --allow-all local.ts ./deno.sock
