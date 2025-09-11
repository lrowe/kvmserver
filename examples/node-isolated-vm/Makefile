.PHONY: build check clean fmt lint test
bench: build
	node --no-node-snapshot index.js hello.js
	node --no-node-snapshot index.js target/renderer.js
build: target/renderer.js node_modules
node_modules:
	npm install
target/renderer.js:
	mkdir -p target
	curl -L https://raw.githubusercontent.com/lrowe/react-server-render-benchmark/8866042d1944e98f8552f78458a553b7d3a94af1/renderer.mjs | sed '252498,$$d' > target/renderer.js
check:
	deno check
clean:
	rm -rf node_modules target
fmt:
	deno fmt
lint:
	deno lint
test: ;
