.PHONY: build check clean fmt lint test
build:
	mkdir -p target
	deno compile \
		--allow-all \
		--v8-flags=-predictable,--stress-maglev,--prepare-always-turbofan,--always-turbofan,--always-sparkplug,--max-old-space-size=64,--max-semi-space-size=64 \
		-o "target/helloworld" \
		'data:,const arg = Deno.args[0] || "8000"; Deno.serve(arg.includes("/") ? { path: arg } : { port: Number(arg) }, () => new Response("Hello, World!"))'
	deno compile \
		--no-check \
		--allow-all \
		--v8-flags=-predictable,--stress-maglev,--prepare-always-turbofan,--always-turbofan,--always-sparkplug,--max-old-space-size=256,--max-semi-space-size=256 \
		-o "target/renderer" \
		https://raw.githubusercontent.com/lrowe/react-server-render-benchmark/8866042d1944e98f8552f78458a553b7d3a94af1/renderer.mjs
	curl -L -o target/deno_city.jpeg https://artwork.deno.dev/images/artwork/deno_city.jpeg
	curl -L -o target/magick.wasm.bin "https://cdn.jsdelivr.net/npm/@imagemagick/magick-wasm@0.0.35/dist/magick.wasm"
	deno compile \
		--allow-all \
		--include target/magick.wasm.bin \
		--include target/deno_city.jpeg \
		--v8-flags=--predictable \
		-o target/imagemagick \
		imagemagick.ts
check:
	deno check
clean:
	rm -rf target
fmt:
	deno fmt
lint:
	deno lint
test:
	deno test --allow-all --quiet .
