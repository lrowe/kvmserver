# KVM Server Deno examples

- Multiple background threads for reading cache files are started when using
  `deno run`. This results in the epoll event loop being run with a timeout
  until those threads complete. This is avoided by using `deno compile` to
  package your dependenices into a single executable and avoid this overhead.

- Warmup the JIT before kvmserver forks, e.g.
  `kvmserver --ephemeral --warmup 1000`, so the worker VMs run optimized JIT
  code.

- Deno is based on V8 which uses background threads for garbage collection and
  JIT compilation by default. This can be avoided by specifying
  `--v8-flags=--predictable`. Using `--v8-flags=--single-threaded` also works
  but requires many more warmup iterations to reach full speed.

- Specify v8 memory limits so that guest process does not grow too large during
  warmup, e.g. `--v8-flags=--max-old-space-size=256,--max-semi-space-size=256`.

- If using `deno run` (not recommended) specify environment variable
  `DENO_NO_UPDATE_CHECK=1` to avoid checking for updates.

- Recommended v8 flags (memory usage will depend on program):

```
--v8-flags=-predictable,--max-old-space-size=256,--max-semi-space-size=256
```

## Avoiding multithreading

TODO: See whether we can optionally disable `spawn_blocking` in Deno.

Various Deno APIs move blocking operations off of the main thread. Avoid:

- Async filesystem calls, e.g. `Deno.readFile`. Prefer `Deno.readFileSync`
  instead.

- CPU heavy `crypto` API calls.

- DNS lookup in `fetch` calls. Specify IP address or use a proxy which performs
  DNS resolution.
