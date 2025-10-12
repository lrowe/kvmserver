const kvmserverguest = Deno.dlopen("libkvmserverguest.so", {
  kvmserverguest_storage_wait_paused: {
    parameters: ["buffer", "isize"],
    result: "isize",
  },
});

let result = 0;
const bufptrptrbuf = new BigUint64Array(1);
const bufptrptr = Deno.UnsafePointer.of(bufptrptrbuf);
const bufptrptrview = new Deno.UnsafePointerView(bufptrptr!);
while (true) {
  // Wait for a UInt8Array buffer from C
  const buflen = Number(
    kvmserverguest.symbols.kvmserverguest_storage_wait_paused(
      bufptrptrbuf,
      BigInt(result),
    ),
  );
  const bufptr = bufptrptrview.getPointer(0);
  if (bufptr === null || buflen < 0) {
    result = -1;
    continue;
  }
  // View it as a Uint8Array
  const arrayBuffer = Deno.UnsafePointerView.getArrayBuffer(bufptr, buflen);
  const buffer = new Uint8Array(arrayBuffer);
  const response = "Hello, World!";
  const { read, written } = new TextEncoder().encodeInto(response, buffer);
  result = read < response.length ? -1 : written;
}
