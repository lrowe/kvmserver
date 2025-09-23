const drogon = Deno.dlopen(
  import.meta.dirname + "/liblocal.so",
  {
    remote_resume: { parameters: ["buffer", "usize"], result: "void" },
  },
);

const arg = Deno.args[0] || "8000";
const options = arg.includes("/") ? { path: arg } : { port: Number(arg) };
Deno.serve(options, () => {
  const remote_buffer = new Int32Array(1);
  const buflen = BigInt(remote_buffer.byteLength);
  let total = 0;
  for (let i = 0; i < 100; i++) {
    drogon.symbols.remote_resume(remote_buffer, buflen);
    total += remote_buffer[0];
  }
  return new Response(`Hello: ${total}`);
});
