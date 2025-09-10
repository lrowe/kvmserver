import process from "node:process";

function handler(_req) {
  return new Response("Hello, World!");
}

const arg = process.argv[2];
const options = arg === undefined
  ? {}
  : Number.isInteger(Number(arg))
  ? { port: Number(arg) }
  : { path: arg, unix: arg };

export default { fetch: handler, ...options };
