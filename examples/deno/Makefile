.PHONY: build check clean fmt lint test
build: target/helloworld target/renderer target/imagemagick target/ca.crt
target/helloworld:
	mkdir -p target
	deno compile \
		--allow-all \
		--v8-flags=-predictable,--max-old-space-size=64,--max-semi-space-size=64 \
		-o target/helloworld \
		'data:,const arg = Deno.args[0] || "8000"; Deno.serve(arg.includes("/") ? { path: arg } : { port: Number(arg) }, () => new Response("Hello, World!"))'
target/renderer:
	mkdir -p target
	deno compile \
		--no-check \
		--allow-all \
		--v8-flags=-predictable,--max-old-space-size=256,--max-semi-space-size=256 \
		-o target/renderer \
		https://raw.githubusercontent.com/lrowe/react-server-render-benchmark/8866042d1944e98f8552f78458a553b7d3a94af1/renderer.mjs
target/deno_city.jpeg:
	mkdir -p target
	curl -L -o target/deno_city.jpeg https://artwork.deno.dev/images/artwork/deno_city.jpeg
target/imagemagick: target/deno_city.jpeg
	mkdir -p target
	deno compile \
		--allow-all \
		--v8-flags=--predictable \
		-o target/imagemagick \
		imagemagick.ts
target/ca.crt target/ca.key target/server.crt target/server/key &:
	mkdir -p target
	cd target && ../mkcert.sh
check:
	deno check
clean:
	rm -rf .cache target
fmt:
	deno fmt
lint:
	deno lint
test:
	deno test --allow-all --quiet .
