console.log("Hello from Deno inside TinyKVM");
const kvmserverguest = Deno.dlopen("libkvmserverguest.so", {
  remote_resume: { parameters: ["buffer", "isize"], result: "isize" },
});

// Simple HTTP server
Deno.serve({ port: 8080 }, (req) => {
  const url = new URL(req.url);
  if (url.pathname === "/") {
    const remote_buffer = new Uint8Array(256);
    const len = Number(kvmserverguest.symbols.remote_resume(
      remote_buffer,
      BigInt(remote_buffer.byteLength),
    ));
    if (len < 0) {
      return new Response("Internal Server Error", { status: 500 });
    }
    const remote_str = new TextDecoder().decode(
      new Uint8Array(remote_buffer.buffer, 0, len),
    );
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
