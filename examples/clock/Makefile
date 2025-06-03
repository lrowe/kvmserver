.PHONY: build check clean fmt lint test
build: target/test
target/test:
	mkdir -p target
	$(CC) -o target/test test.cpp 
check: ;
clean:
	rm -rf target
fmt: ;
lint: ;
test: build
	deno test --allow-all --quiet .
