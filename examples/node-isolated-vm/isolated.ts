import http from "node:http";
import ivm from "isolated-vm";
import fs from "node:fs";
import process from "node:process";

const [_bin, _this_script, filename, port = "8000"] = process.argv;
const code = fs.readFileSync(filename, "utf-8");
const run = `handler_body()`;
// more than one warmup makes no difference to performance.
const snapshot = ivm.Isolate.createSnapshot([{ code }], run);
http.createServer((_req, res) => {
  const isolate = new ivm.Isolate({ snapshot });
  const context = isolate.createContextSync();
  const result = context.evalSync(run);
  res.end(result);
  context.release();
  isolate.dispose();
}).listen(Number(port));
console.log(`Listening on ${port}`);
