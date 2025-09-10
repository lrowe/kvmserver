// Forked processes lack their ancillary threads so switch off concurrent GC/JIT:
// BUN_GC_TIMER_DISABLE=true BUN_JSC_useConcurrentGC=false BUN_JSC_useConcurrentJIT=false

import { dlopen } from "bun:ffi";
import { gc, resolveSync, type ServeOptions, type UnixServeOptions } from "bun";
import process from "node:process";
import fs from "node:fs";

type Handler = (request: Request) => Response | Promise<Response>;

const AF_UNIX = 1;
const AF_INET = 2;
const PR_SET_PDEATHSIG = 1;
const SHUT_WR = 1;
const SIGTERM = 15;
const SOCK_STREAM = 1;

const libcPath = process.platform === "darwin" ? "libc.dylib" : "libc.so.6";
const libc = dlopen(libcPath, {
  accept: {
    // sockfd, addr, addrlen
    args: ["i32", "pointer", "pointer"],
    returns: "i32",
  },
  bind: {
    // socket_fd, sockaddr_ptr, addrlen
    args: ["i32", "buffer", "u32"],
    returns: "i32",
  },
  fork: {
    args: [],
    returns: "i32",
  },
  getpid: {
    args: [],
    returns: "i32",
  },
  getppid: {
    args: [],
    returns: "i32",
  },
  listen: {
    // sockfd, backlog
    args: ["i32", "i32"],
    returns: "i32",
  },
  prctl: {
    args: ["i32", "i32"],
    returns: "i32",
  },
  recv: {
    //sockfd, buf, size, flags
    args: ["i32", "buffer", "u64", "i32"],
    returns: "i32",
  },
  shutdown: {
    // sockfd, how
    args: ["i32", "i32"],
    returns: "i32",
  },
  socket: {
    // domain, type, protocol
    args: ["i32", "i32", "i32"],
    returns: "i32",
  },
  waitpid: {
    // pid, status, options
    args: ["i32", "pointer", "i32"],
    returns: "i32",
  },
});

function bind(
  { port, unix }: ServeOptions | UnixServeOptions,
): [fd: number, addr: string] {
  let sockaddr;
  let af;
  let addr;
  if (unix !== undefined) {
    addr = unix;
    const chars = new TextEncoder().encode(unix + "\0");
    // struct sockaddr_un
    sockaddr = new Uint8Array(2 + chars.byteLength);
    af = new Uint16Array(sockaddr.buffer, 0, 1)[0] = AF_UNIX;
    sockaddr.set(chars, 2);
  } else {
    // struct sockaddr_in
    sockaddr = new Uint8Array(16);
    af = new Uint16Array(sockaddr.buffer, 0, 1)[0] = AF_INET;
    const dv = new DataView(sockaddr.buffer);
    const p = Number(port ?? process.env.BUN_PORT ?? "3000");
    addr = `0.0.0.0:%{p}`;
    dv.setUint16(2, p, false); // big-endian
  }
  const fd = libc.symbols.socket(af, SOCK_STREAM, 0);
  if (fd < 0) {
    throw new Error("unable to open socket");
  }
  if (libc.symbols.bind(fd, sockaddr, sockaddr.byteLength) == -1) {
    throw new Error("unable to bind socket");
  }
  return [fd, addr];
}

function listen(fd: number) {
  if (libc.symbols.listen(fd, 512) < 0) {
    throw new Error("listen failed");
  }
}

async function handleConnection(handler: Handler, conn: number) {
  // Unfortunately Bun.serve does not support serving on fds.
  // This also applies to Bun's implementation of node:http and node:net.
  const buf = new Uint8Array(1024);
  const bytesRead = libc.symbols.recv(conn, buf, buf.byteLength, 0);
  if (bytesRead !== 0) {
    let response, bytes;
    try {
      response = await handler(new Request("http://localhost/"));
      bytes = await response.bytes();
    } catch (_err) {
      response = new Response("Internal Server Error", {
        status: 500,
        statusText: "Internal Server Error",
      });
      bytes = await response.bytes();
    }
    response.headers.set("connection", "close");
    const head = `HTTP/1.1 ${response.status} ${
      response.statusText || "OK"
    }\r\n${
      Array.from(response.headers, ([k, v]) => `${k}: ${v}\r\n`).join()
    }\r\n`;
    fs.writeFileSync(conn, new TextEncoder().encode(head));
    fs.writeFileSync(conn, bytes);
  }
  if (libc.symbols.shutdown(conn, SHUT_WR) < 0) {
    console.error("shutdown failed");
  }
  fs.closeSync(conn);
}

async function runForked(handler: Handler, fd: number) {
  const ppid_before_fork = libc.symbols.getpid();
  while (true) {
    const pid = libc.symbols.fork();
    if (pid === 0) {
      if (libc.symbols.prctl(PR_SET_PDEATHSIG, SIGTERM) === -1) {
        console.error("prctl failed");
        process.exit(1);
      }
      if (libc.symbols.getppid() != ppid_before_fork) {
        process.exit(0);
      }
      // Child
      const conn = libc.symbols.accept(fd, null, null);
      if (conn < 0) {
        console.error("accept failed");
        process.exit(1);
      }
      try {
        await handleConnection(handler, conn);
      } catch (err) {
        console.error(err);
      }
      process.exit(0);
    } else if (pid > 0) {
      // Parent
      if (libc.symbols.waitpid(pid, null, 0) == -1) {
        throw new Error("waitpid() failed");
      }
    } else {
      throw new Error("fork() failed.");
    }
  }
}

async function main(
  options: ServeOptions | UnixServeOptions,
  warmup: number = 100,
) {
  const [fd, addr] = bind(options);
  const handler = options.fetch as Handler;
  for (let i = 0; i < warmup; i++) {
    handler(new Request("http://localhost/"));
  }
  gc(true);
  try {
    listen(fd);
    console.log(`Started process forking server: ${addr}`);
    await runForked(handler, fd);
  } catch (err) {
    console.error(err);
    process.exit(1);
  }
}

if (import.meta.main) {
  const [_bin, _this_script, filename, arg] = process.argv;
  const mod = await import(resolveSync(filename, process.cwd()));
  const listenOptions = arg === undefined
    ? {}
    : Number.isInteger(Number(arg))
    ? { port: Number(arg), unix: undefined }
    : { port: undefined, unix: arg, path: arg };
  const options = { ...mod.default, ...listenOptions };
  await main(options, 100);
}
