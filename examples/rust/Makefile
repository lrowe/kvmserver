.PHONY: build check clean fmt lint test
build: target/release/httpserver target/release/httpserversync
target/release/httpserver target/release/httpserversync: Cargo.lock Cargo.toml $(wildcard src/**/*.rs)
	cargo build --release
check:
	cargo check
clean:
	rm -rf target
fmt:
	cargo fmt
lint:
	cargo clippy
test: build
	deno test --allow-all --quiet .
