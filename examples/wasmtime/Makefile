.PHONY: build check clean fmt lint test
build: ext/hello-wasi-http/target/wasm32-wasip2/release/hello_wasi_http.wasm target/helloworld.wasm target/helloworld-incoming.wasm target/renderer.wasm target/renderer-incoming.wasm
ext/hello-wasi-http/target/wasm32-wasip2/release/hello_wasi_http.wasm:
	cd ext/hello-wasi-http && cargo build --release --target wasm32-wasip2
node_modules/.bin/jco:
	npm install
	# https://github.com/bytecodealliance/jco/pull/956
	sed -i 's/  const nanoseconds = Number(ns % seconds);/  const nanoseconds = Number(ns % nsMagnitude);/g' node_modules/@bytecodealliance/preview2-shim/lib/nodejs/filesystem.js
target/helloworld.wasm: node_modules/.bin/jco helloworld.js
	mkdir -p target
	node_modules/.bin/jco componentize helloworld.js --wit wit/ --world-name component --out target/helloworld.wasm
target/helloworld-incoming.wasm: node_modules/.bin/jco helloworld.js incoming.js
	mkdir -p target
	cat helloworld.js incoming.js > target/helloworld-incoming.js
	node_modules/.bin/jco componentize target/helloworld-incoming.js --wit wit/ --world-name component --out target/helloworld-incoming.wasm
target/renderer.js:
	mkdir -p target
	cat compat.js > target/renderer.js
	sed '252498,$$d' < ../ext/react-server-render-benchmark/renderer.mjs >> target/renderer.js
	echo 'addEventListener("fetch", (event) => event.respondWith(handler(event.request)));' >> target/renderer.js
target/renderer.wasm: node_modules/.bin/jco target/renderer.js
	mkdir -p target
	node_modules/.bin/jco componentize target/renderer.js --wit wit/ --world-name component --out target/renderer.wasm
target/renderer-incoming.wasm: node_modules/.bin/jco target/renderer.js incoming.js
	mkdir -p target
	cat target/renderer.js incoming.js > target/renderer-incoming.js
	node_modules/.bin/jco componentize target/renderer-incoming.js --wit wit/ --world-name component --out target/renderer-incoming.wasm
check:
	deno check
clean:
	rm -rf ext/hello-wasi-http/target node_modules target
fmt:
	deno fmt
lint:
	deno lint
test: ;
