set -e
pushd ../../.build
make -j8
popd

export DENO_V8_FLAGS=--predictable,--max-old-space-size=64,--max-semi-space-size=64
export DENO_NO_UPDATE_CHECK=1
exec ../../.build/kvmserver -e -t 1 --warmup 1000 --allow-all storage --1-to-1 deno run --allow-all remote.ts ++ run deno run --allow-all local.ts
