import { dlopen, FFIType } from "bun:ffi";
import process from "node:process";

const libcPath = process.platform === "darwin" ? "libc.dylib" : "libc.so.6";
const libc = dlopen(libcPath, {
  fork: {
    args: [],
    returns: FFIType.int,
  },
  waitpid: {
    args: [FFIType.int, FFIType.ptr, FFIType.int],
    returns: FFIType.int,
  },
});

const [_bin, _this_script, filename] = process.argv;

const mod = await import(filename);
const handler = mod.default.fetch;

for (let i = 0; i < 1000; i++) {
  handler();
}

for (let i = 0; i < 10; i++) {
  const a = performance.now();
  const _result = handler();
  const d = performance.now() - a;
  console.log(`(warmup) render: ${Math.floor(d * 1000)} µs`);
}

for (let i = 0; i < 10; i++) {
  const start = performance.now();
  const pid = libc.symbols.fork();

  if (pid === 0) {
    // Child
    const a = performance.now();
    const _result = handler();
    const b = performance.now();
    const forktime = Math.floor((a - start) * 1000);
    const render = Math.floor((b - a) * 1000);
    const total = Math.floor((b - start) * 1000);
    process.stdout.write(
      `(child) fork: ${forktime} µs render: ${render} µs total: ${total} µs`,
    );
    process.exit(0);
  } else if (pid > 0) {
    // Parent
    if (libc.symbols.waitpid(pid, null, 0) == -1) {
      console.error("waitpid() failed");
    }
    const duration = performance.now() - start;
    process.stdout.write(
      ` (parent) waitpid: ${Math.floor(duration * 1000)} µs\n`,
    );
  } else {
    console.error("fork() failed.");
  }
}
process.exit(0);
