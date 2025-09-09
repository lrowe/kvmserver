# Benchmarking comparison with wasmtime serve

Reuse comparisons use wasmtime built from Alex Chrichton's experimental
instance-reuse branch at
https://github.com/alexcrichton/wasmtime/commit/e985d36161da43cfb9a8d7c581c6a7a91378a57e

## Rust guest

- https://github.com/sunfishcode/hello-wasi-http

## ComponentJS / StarlingMonkey guests

Adapted from:

- https://github.com/bytecodealliance/jco/tree/main/examples/components/http-hello-world
- https://github.com/bytecodealliance/jco/tree/main/examples/components/http-server-fetch-handler

(Fetch handler examples not compatible with instance reuse.)
