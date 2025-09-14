.PHONY: build check clean fmt lint test
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
