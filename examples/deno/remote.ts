import { connect } from "@db/redis";

const drogon = Deno.dlopen(
  import.meta.dirname + "/libstorage.so",
  {
    wait_for_storage_task_paused: { parameters: [], result: "pointer" },
  },
);
console.log("Hello from Deno Storage inside TinyKVM");

const redisClient = await connect({ hostname: "127.0.0.1", port: 6379 });
while (true) {
  // Wait for a UInt8Array buffer from C
  const bufptr = drogon.symbols.wait_for_storage_task_paused();
  if (bufptr === null) {
    throw new Error("bufptr is null");
  }
  // View it as a Uint8Array of length 256
  const arrayBuffer = Deno.UnsafePointerView.getArrayBuffer(bufptr, 256);
  const buffer = new Uint8Array(arrayBuffer);

  const redis_answer = await redisClient.get("hoge");
  // Copy redis_answer to buffer
  const response = "Hello from Deno storage inside TinyKVM, redis answer: " +
    redis_answer;
  const encoder = new TextEncoder();
  let encoded = encoder.encode(response);
  // Copy to buffer, but leave space for null terminator
  if (encoded.length > 255) {
    encoded = encoded.slice(0, 255);
  }
  buffer.set(encoded); // Leave space for null terminator
  buffer[encoded.length] = 0; // Null-terminate
}
