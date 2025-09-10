# Benchmarking comparison with Bun process forking

Bun is based on JSC which (unlike V8) is not completely incompatible with
process forking.

This is not expected to be completely reliable as Bun does not have a single
threaded mode. Forking a multithreaded process risks deadlocks as only the
thread that called fork exists in the new process.

The following flags disable the use of multithreaded GC and JIT:
```
BUN_GC_TIMER_DISABLE=true BUN_JSC_useConcurrentGC=false BUN_JSC_useConcurrentJIT=false
```
