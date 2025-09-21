set -e
pushd ../../.build
make -j8
popd
../../.build/kvmserver -e --storage --allow-all ~/.deno/bin/deno -- run --allow-all local.ts
