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
