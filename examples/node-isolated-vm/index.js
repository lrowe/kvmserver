#!/usr/bin/env -S node --no-node-snapshot
const ivm = require("isolated-vm");
const fs = require("node:fs");
const [_node_bin, _this_script, filename] = process.argv;
const code = fs.readFileSync(filename, "utf-8");
const run = `handler_body()`;

// more than one warmup makes no difference to performance.
const snapshot = ivm.Isolate.createSnapshot([{ code }], run);
const times = Array.from({ length: 100 }, () => {
  const start = performance.now();
  const isolate = new ivm.Isolate({ snapshot });
  const context = isolate.createContextSync();
  const result = context.evalSync(run);
  const { cpuTime, wallTime } = isolate;
  context.release();
  isolate.dispose();
  return performance.now() - start;
}).toSorted();
const averagems = times.reduce((a, b) => a + b, 0) / times.length;
const p50ms = times[49];
const p90ms = times[89];
const p99ms = times[98];
console.log({ filename, averagems, p50ms, p90ms, p99ms });
