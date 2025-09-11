import http from "node:http";
import fs from "node:fs";
import process from "node:process";

const [_bin, _this_script, filename, port = "8000"] = process.argv;
// Must be an indirect eval.
eval?.(fs.readFileSync(filename, "utf-8"));
declare function handler_body(): string;

http.createServer((_req, res) => {
  const result = handler_body();
  res.end(result);
}).listen(Number(port));
console.log(`Listening on ${port}`);
