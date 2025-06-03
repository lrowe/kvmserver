.PHONY: build check clean fmt lint test
build: target/lune
target/lune:
	mkdir -p target
	curl -L -o target/lune.zip https://github.com/lune-org/lune/releases/download/v0.9.3/lune-0.9.3-linux-x86_64.zip
	cd target && unzip lune.zip
check: ;
clean:
	rm -rf target 
fmt: ;
lint: ;
test: build
	deno test --allow-all --quiet .
