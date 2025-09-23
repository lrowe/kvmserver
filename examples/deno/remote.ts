const drogon = Deno.dlopen(
  import.meta.dirname + "/libstorage.so",
  {
    wait_for_storage_task_paused: { parameters: [], result: "pointer" },
  },
);

let i = 0;
while (true) {
  const bufptr = drogon.symbols.wait_for_storage_task_paused();
  if (bufptr === null) {
    throw new Error("bufptr is null");
  }
  const arrayBuffer = Deno.UnsafePointerView.getArrayBuffer(bufptr, 4);
  const buffer = new Int32Array(arrayBuffer);
  buffer[0] = i++;
}
