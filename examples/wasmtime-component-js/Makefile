.PHONY: build check clean fmt lint test
build: target/helloworld.wasm target/helloworldaot.wasm target/renderer.wasm
node_modules/.bin/componentize-js:
	npm install
	# https://github.com/bytecodealliance/jco/pull/956
	sed -i 's/  const nanoseconds = Number(ns % seconds);/  const nanoseconds = Number(ns % nsMagnitude);/g' node_modules/@bytecodealliance/preview2-shim/lib/nodejs/filesystem.js
target/helloworld.wasm: node_modules/.bin/componentize-js
	mkdir -p target
	node_modules/.bin/componentize-js -w wit -o target/helloworld.wasm helloworld.js
target/helloworldaot.wasm: node_modules/.bin/componentize-js
	mkdir -p target
	node_modules/.bin/componentize-js -w wit -o target/helloworldaot.wasm helloworld.js --aot
target/renderer.mjs:
	mkdir -p target
	curl -Lo target/renderer.mjs https://raw.githubusercontent.com/lrowe/react-server-render-benchmark/f754693c9cb6ad4f492039897d3444dec3103691/renderer.mjs
target/renderer.wasm: node_modules/.bin/componentize-js target/renderer.mjs
	mkdir -p target
	node_modules/.bin/componentize-js -w wit -o target/renderer.wasm target/renderer.mjs
check: ;
clean:
	rm -rf node_modules target
fmt: ;
lint: ;
test: ;
