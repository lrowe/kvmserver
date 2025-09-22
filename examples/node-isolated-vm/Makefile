.PHONY: build check clean fmt lint test
build: target/renderer.js node_modules
node_modules:
	npm install
target/renderer.js:
	mkdir -p target
	sed '252498,$$d' < ../ext/react-server-render-benchmark/renderer.mjs > target/renderer.js
check:
	deno check
clean:
	rm -rf node_modules target
fmt:
	deno fmt
lint:
	deno lint
test: ;
