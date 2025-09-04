.PHONY: build check clean fmt lint test
bench: build
	bun fork.js ./hello.js
	bun fork.js ./target/renderer.mjs
build: target/renderer.mjs
target/renderer.mjs:
	mkdir -p target
	curl -Lo target/renderer.mjs https://raw.githubusercontent.com/lrowe/react-server-render-benchmark/8866042d1944e98f8552f78458a553b7d3a94af1/renderer.mjs
check: ;
clean:
	rm -rf target
fmt: ;
lint: ;
test: ;
