.PHONY: build check clean format test
build:
	uv sync --frozen
check: ;
clean:
	rm -rf __pycache__ .venv .ruff_cache
fmt: build
	.venv/bin/ruff format
lint: build
	.venv/bin/ruff check
test: build
	deno test --allow-all --quiet .
