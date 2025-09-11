import { bind, handleConnection, type Handler, listen } from "./fork.ts";
import { gc, resolveSync, type ServeOptions, type UnixServeOptions } from "bun";
import process from "node:process";

async function main(
  options: ServeOptions | UnixServeOptions,
  warmup: number = 100,
) {
  const [fd, addr] = bind(options);
  const handler = options.fetch as Handler;
  for (let i = 0; i < warmup; i++) {
    handler(new Request("http://localhost/"));
  }
  gc(true);
  try {
    listen(fd);
    console.log(`Started main thread server: ${addr}`);
    while (true) {
      await handleConnection(handler, fd);
    }
  } catch (err) {
    console.error(err);
    process.exit(1);
  }
}

if (import.meta.main) {
  const [_bin, _this_script, filename, arg] = process.argv;
  const mod = await import(resolveSync(filename, process.cwd()));
  const listenOptions = arg === undefined
    ? {}
    : Number.isInteger(Number(arg))
    ? { port: Number(arg), unix: undefined }
    : { port: undefined, unix: arg, path: arg };
  const options = { ...mod.default, ...listenOptions };
  await main(options, 100);
}
