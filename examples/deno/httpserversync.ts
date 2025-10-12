export const AF_UNIX = 1;
export const AF_INET = 2;
export const SHUT_WR = 1;
export const SO_REUSEADDR = 2;
export const SOCK_STREAM = 1;
export const SOCK_DGRAM = 2;
export const SOCK_SEQPACKET = 5;
export const SOL_SOCKET = 1;

export const libc = Deno.dlopen("libc.so.6", {
  accept4: {
    parameters: ["i32", "pointer", "pointer", "i32"] as [
      sockfd: "i32",
      addr: "pointer",
      addrlen: "pointer",
      flags: "i32",
    ],
    result: "i32",
  },
  bind: {
    parameters: ["i32", "buffer", "u32"] as [
      sockfd: "i32",
      addr: "buffer",
      addrlen: "u32",
    ],
    result: "i32",
  },
  close: {
    parameters: ["i32"] as [fd: "i32"],
    result: "i32",
  },
  connect: {
    parameters: ["i32", "buffer", "u32"] as [
      sockfd: "i32",
      addr: "buffer",
      addrlen: "u32",
    ],
    result: "i32",
  },
  errno: { type: "pointer" },
  listen: {
    parameters: ["i32", "i32"] as [sockfd: "i32", backlog: "i32"],
    result: "i32",
  },
  recv: {
    parameters: ["i32", "buffer", "i64", "i32"] as [
      sockfd: "i32",
      buf: "buffer",
      size: "i64",
      flags: "i32",
    ],
    result: "i64",
  },
  send: {
    parameters: ["i32", "buffer", "i64", "i32"] as [
      sockfd: "i32",
      buf: "buffer",
      size: "i64",
      flags: "i32",
    ],
    result: "i64",
  },
  setsockopt: {
    parameters: ["i32", "i32", "i32", "buffer", "u32"] as [
      sockfd: "i32",
      level: "i32",
      optname: "i32",
      optval: "buffer",
      optlen: "u32",
    ],
    result: "i64",
  },
  shutdown: {
    parameters: ["i32", "i32"] as [sockfd: "i32", how: "i32"],
    result: "i32",
  },
  socket: {
    parameters: ["i32", "i32", "i32"] as [
      domain: "i32",
      type: "i32",
      protocol: "i32",
    ],
    result: "i32",
  },
  strerror: {
    parameters: ["i32"] as [errnum: "i32"],
    result: "pointer",
  },
});

const errnoPtr = new Deno.UnsafePointerView(libc.symbols.errno!);
export const errno = () => errnoPtr.getInt32();
export const strerror = (errnum: number = errno()) => {
  const ptr = libc.symbols.strerror(errnum);
  return ptr === null ? "" : new Deno.UnsafePointerView(ptr).getCString();
};
class LibcError extends Error {
  constructor(message: string) {
    super(`${message}: ${strerror()}`);
  }
}

export function sockaddr_in(port: number): Uint8Array<ArrayBuffer> {
  const sockaddr = new Uint8Array(16);
  new Uint16Array(sockaddr.buffer, 0, 1)[0] = AF_INET;
  const dv = new DataView(sockaddr.buffer);
  dv.setUint16(2, port, false); // big-endian
  return sockaddr;
}

export function sockaddr_un(path: string): Uint8Array<ArrayBuffer> {
  const chars = new TextEncoder().encode(
    path.charCodeAt(0) === 0 ? path : path + "\0",
  );
  const sockaddr = new Uint8Array(2 + chars.byteLength);
  new Uint16Array(sockaddr.buffer, 0, 1)[0] = AF_UNIX;
  sockaddr.set(chars, 2);
  return sockaddr;
}

export function serve(listenfd: number) {
  const BUFFER_SIZE = 4096;
  const buf = new Uint8Array(BUFFER_SIZE);
  while (true) {
    const connfd = libc.symbols.accept4(listenfd, null, null, 0);
    if (connfd < 0) {
      throw new LibcError("accept");
    }
    try {
      const bytesRead = Number(libc.symbols.recv(
        connfd,
        buf,
        BigInt(buf.byteLength),
        0,
      ));
      if (bytesRead > 0) {
        let value;
        // Array.from("GET ", c => c.charCodeAt(0)) // [ 71, 69, 84, 32 ]
        if (buf[0] !== 71 || buf[1] !== 69 || buf[2] !== 84 || buf[3] !== 32) {
          console.error(
            `--- Bad request ---\n${
              new TextDecoder("latin1").decode(buf)
            }\n-------------------`,
          );
          value = "HTTP/1.1 405 Method Not Allowed\r\n" +
            "Connection: close\r\n" +
            "Content-Type: text/plain; charset=utf-8\r\n" +
            "\r\n" +
            "Method Not Allowed";
        } else {
          value = "HTTP/1.1 200 OK\r\n" +
            "Connection: close\r\n" +
            "Content-Type: text/plain; charset=utf-8\r\n" +
            "\r\n" +
            "Hello, World!";
        }
        const { written } = new TextEncoder().encodeInto(value, buf);
        let totalSent = 0;
        while (totalSent < written) {
          const wbuf = new Uint8Array(
            buf.buffer,
            totalSent,
            written - totalSent,
          );
          const bytesSent = Number(
            libc.symbols.send(connfd, wbuf, BigInt(wbuf.length), 0),
          );
          if (bytesSent > 0) {
            totalSent += bytesSent;
            continue;
          } else if (bytesSent === 0) {
            break;
          } else {
            throw new LibcError("send");
          }
        }
      } else if (bytesRead === 0) {
        break;
      } else {
        throw new LibcError("recv");
      }
    } finally {
      if (libc.symbols.shutdown(connfd, SHUT_WR) < 0) {
        console.error(`shutdown: ${strerror()}`);
      }
      if (libc.symbols.close(connfd) < 0) {
        console.error(`close: ${strerror()}`);
      }
    }
  }
}

if (import.meta.main) {
  const [arg = "8000"] = Deno.args;
  const sockaddr = arg.includes("/")
    ? sockaddr_un(arg)
    : sockaddr_in(Number(arg));
  const listenfd = libc.symbols.socket(
    new Uint16Array(sockaddr.buffer, 0, 1)[0],
    SOCK_STREAM,
    0,
  );
  if (listenfd < 0) {
    throw new LibcError("socket");
  }
  const reuse = new Uint32Array([1]);
  if (
    libc.symbols.setsockopt(
      listenfd,
      SOL_SOCKET,
      SO_REUSEADDR,
      reuse,
      reuse.byteLength,
    ) < 0
  ) {
    throw new LibcError("setsockopt");
  }
  if (libc.symbols.bind(listenfd, sockaddr, sockaddr.byteLength) < 0) {
    throw new LibcError("bind");
  }
  if (libc.symbols.listen(listenfd, 128) < 0) {
    throw new LibcError("listen");
  }
  console.error(`Listening on: ${arg}`);
  serve(listenfd);
}
