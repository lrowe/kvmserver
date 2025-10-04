//import { connect } from "@db/redis";

const kvmserverguest = Deno.dlopen("libkvmserverguest.so", {
  storage_wait_paused: { parameters: ["buffer", "isize"], result: "isize" },
});
console.log("Hello from Deno Storage inside TinyKVM");

//const redisClient = await connect({ hostname: "127.0.0.1", port: 6379 });
let result = 0;
const bufptrptrbuf = new BigUint64Array(1);
const bufptrptr = Deno.UnsafePointer.of(bufptrptrbuf);
const bufptrptrview = new Deno.UnsafePointerView(bufptrptr!);
while (true) {
  // Wait for a UInt8Array buffer from C
  const buflen = Number(
    kvmserverguest.symbols.storage_wait_paused(bufptrptrbuf, BigInt(result)),
  );
  const bufptr = bufptrptrview.getPointer(0);
  if (bufptr === null || buflen < 0) {
    result = -1;
    continue;
  }
  // View it as a Uint8Array of length 256
  const arrayBuffer = Deno.UnsafePointerView.getArrayBuffer(bufptr, buflen);
  const buffer = new Uint8Array(arrayBuffer);

  //const redis_answer = await redisClient.get("hoge");
  const redis_answer = "await redisClient.ping();";
  // Copy redis_answer to buffer
  const response = "Hello from Deno storage inside TinyKVM, redis answer: " +
    redis_answer;
  const { read, written } = new TextEncoder().encodeInto(response, buffer);
  result = read < response.length ? -1 : written;
}
