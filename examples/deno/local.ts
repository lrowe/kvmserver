console.log("Hello from Deno inside TinyKVM");
const drogon = Deno.dlopen(
  import.meta.dirname + "/liblocal.so",
  {
    remote_resume: { parameters: ["buffer", "usize"], result: "void" },
  },
);
function getZeroTerminatedString(buffer: Uint8Array, encoding = "utf8") {
  const nullByteIndex = buffer.indexOf(0x00);
  const slice = nullByteIndex !== -1 ? buffer.slice(0, nullByteIndex) : buffer;
  return new TextDecoder(encoding).decode(slice);
}
function getRemoteString(): string {
  const remote_buffer = new Uint8Array(256);
  // Pass a number instead of a bigint to benefit from Fast API
  // https://denonomicon.deno.dev/types/64-bit-integers
  drogon.symbols.remote_resume(
    remote_buffer,
    remote_buffer.byteLength as unknown as bigint,
  );

  // Get remote_buffer as a zero-terminated string
  return getZeroTerminatedString(remote_buffer);
}

// Simple HTTP server
Deno.serve({ port: 8080 }, (req) => {
  const url = new URL(req.url);
  if (url.pathname === "/") {
    const remote_str = getRemoteString();
    return new Response(
      "Hello from Deno inside TinyKVM\n" + remote_str + "\n",
      {
        headers: { "content-type": "text/plain; charset=utf-8" },
      },
    );
  } else {
    return new Response("Not Found\n", { status: 404 });
  }
});
