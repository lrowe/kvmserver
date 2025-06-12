export DENO_V8_FLAGS=--predictable,--stress-maglev,--prepare-always-turbofan,--always-turbofan,--always-sparkplug,--max-old-space-size=256,--max-semi-space-size=256
export DENO_NO_UPDATE_CHECK=1
source ./run.sh --allow-all --warmup 1000 --ephemeral --threads 2 -- deno run --allow-all \
  https://raw.githubusercontent.com/lrowe/react-server-render-benchmark/refs/heads/main/renderer.mjs \
  "$@"
