DENO_V8_FLAGS=--predictable,--stress-maglev,--prepare-always-turbofan,--always-turbofan,--always-sparkplug,--max-old-space-size=256,--max-semi-space-size=256 \
DENO_NO_UPDATE_CHECK=1 \
./run.sh --allow-all -w 1000 -e -t 2 -p deno -- run --allow-all \
  https://raw.githubusercontent.com/lrowe/react-server-render-benchmark/refs/heads/main/renderer.mjs \
  "$@"
