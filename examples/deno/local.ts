const kvmserverguest = Deno.dlopen("libkvmserverguest.so", {
  kvmserverguest_remote_resume: {
    parameters: ["buffer", "isize"],
    result: "isize",
  },
});

Deno.serve({ port: 8000 }, (_req) => {
  const remote_buffer = new Uint8Array(256);
  const len = Number(kvmserverguest.symbols.kvmserverguest_remote_resume(
    remote_buffer,
    BigInt(remote_buffer.byteLength),
  ));
  if (len < 0) {
    return new Response("Internal Server Error", { status: 500 });
  }
  const remote_str = new TextDecoder().decode(
    new Uint8Array(remote_buffer.buffer, 0, len),
  );
  return new Response(remote_str);
});
