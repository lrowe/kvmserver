console.log("Hello from Deno inside TinyKVM");
const kvmserverguest = Deno.dlopen("libkvmserverguest.so", {
  remote_resume: { parameters: ["buffer", "usize"], result: "void" },
});
function getRemoteString(): string {
  const remote_buffer = new Uint8Array(256);

  kvmserverguest.symbols.remote_resume(
    remote_buffer,
    BigInt(remote_buffer.byteLength),
  );
  return new Deno.UnsafePointerView(Deno.UnsafePointer.of(remote_buffer)!)
    .getCString();
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
