import { assertEquals } from "@std/assert";
import { kvmServerCommand, waitForLine } from "./testutil.ts";

const path = "./deno.sock";
const args = [path];
const cwd = import.meta.dirname;
const ephemeral = true;
const allowAll = true;
const warmup = 1000;
const duration = Deno.args[0] ?? "1s";

type OhaPercentiles = {
  "p10": number;
  "p25": number;
  "p50": number;
  "p75": number;
  "p90": number;
  "p95": number;
  "p99": number;
  "p99.9": number;
  "p99.99": number;
};

type OhaResults = {
  "summary": {
    "successRate": number;
    "total": number;
    "slowest": number;
    "fastest": number;
    "average": number;
    "requestsPerSec": number;
    "totalData": number;
    "sizePerRequest": number;
    "sizePerSec": number;
  };
  "responseTimeHistogram": Record<number, number>;
  "latencyPercentiles": OhaPercentiles;
  "rps": {
    "mean": number;
    "stddev": number;
    "max": number;
    "min": number;
    "percentiles": OhaPercentiles;
  };
  "details": Record<"DNSDialup" | "DNSLookup", {
    "average": number;
    "fastest": number;
    "slowest": number;
  }>;
  "statusCodeDistribution": Record<number, number>;
  "errorDistribution": Record<string, number>;
};

async function oha(
  unixSocket: string,
  ...ohaArgs: string[]
): Promise<OhaResults> {
  const command = new Deno.Command("oha", {
    args: [
      ...ohaArgs,
      "--wait-ongoing-requests-after-deadline",
      "--no-tui",
      "--json",
      `--unix-socket=${unixSocket}`,
      "http://127.0.0.1:8000/",
    ],
  });
  const output = await command.output();
  return JSON.parse(new TextDecoder("utf-8").decode(output.stdout));
}

function waitListening(proc: Deno.ChildProcess) {
  return Promise.race([
    waitForLine(proc.stderr, (line) => line.startsWith("Listening")),
    proc.status.then(({ code }) => {
      throw new Error(`Status code: ${code}`);
    }),
  ]);
}

function waitProgram(proc: Deno.ChildProcess) {
  return Promise.race([
    waitForLine(proc.stdout, (line) => line.startsWith("Program")),
    proc.status.then(({ code }) => {
      throw new Error(`Status code: ${code}`);
    }),
  ]);
}

type NamedOhaResults = { name: string } & OhaResults;

class BenchGroup {
  groupName: string;
  rows: NamedOhaResults[] = [];
  cols: Record<string, (row: NamedOhaResults) => string> = {
    name: (r) => r.name,
    average: (r) => `${Math.round(r.summary.average * 1_000_000)} µs`,
    p50: (r) => `${Math.round(r.latencyPercentiles.p50 * 1_000_000)} µs`,
    p90: (r) => `${Math.round(r.latencyPercentiles.p90 * 1_000_000)} µs`,
    p99: (r) => `${Math.round(r.latencyPercentiles.p99 * 1_000_000)} µs`,
  };

  constructor(groupName: string) {
    this.groupName = groupName;
  }

  async bench(
    name: string,
    command: Deno.Command,
    ready: (proc: Deno.ChildProcess) => Promise<void>,
    ohaArgs: string[] = [],
    warmupFn?: () => Promise<void>,
  ) {
    try {
      await using proc = command.spawn();
      await ready(proc);
      await warmupFn?.();
      const results = await oha(path, ...ohaArgs);
      assertEquals(results.errorDistribution, {}, "errorDistribution");
      this.rows.push({ name, ...results });
    } finally {
      await Deno.remove(path).catch(() => {});
    }
  }

  toString() {
    return [
      `### ${this.groupName}`,
      `| ${Object.keys(this.cols).join(" | ")} |`,
      `| ${Object.keys(this.cols).map(() => "---").join(" | ")} |`,
      ...this.rows.map((r) =>
        `| ${Object.values(this.cols).map((fn) => fn(r)).join(" | ")} |`
      ),
    ].join("\n");
  }
}

async function addBenches(groupName: string, program: string) {
  const group = new BenchGroup(groupName);

  await group.bench(
    "base (reusing connection)",
    new Deno.Command(program, { args, cwd, stderr: "piped" }),
    waitListening,
    ["-c=1", `-z=${duration}`],
    async () => {
      await oha(path, "-c=1", `-n=${warmup}`);
    },
  );

  await group.bench(
    "base",
    new Deno.Command(program, { args, cwd, stderr: "piped" }),
    waitListening,
    ["--disable-keepalive", "-c=1", `-z=${duration}`],
    async () => {
      await oha(path, "-c=1", `-n=${warmup}`);
    },
  );

  await group.bench(
    "kvmserver threads=1",
    kvmServerCommand({ program, args, cwd, allowAll, warmup }),
    waitProgram,
    ["--disable-keepalive", "-c=1", `-z=${duration}`],
  );

  await group.bench(
    "kvmserver ephemeral threads=1",
    kvmServerCommand({ program, args, cwd, allowAll, warmup, ephemeral }),
    waitProgram,
    ["--disable-keepalive", "-c=1", `-z=${duration}`],
  );

  await group.bench(
    "kvmserver ephemeral threads=2",
    kvmServerCommand({
      program,
      args,
      cwd,
      allowAll,
      warmup,
      ephemeral,
      threads: 2,
    }),
    waitProgram,
    ["--disable-keepalive", "-c=2", `-z=${duration}`],
  );

  await group.bench(
    "kvmserver ephemeral threads=4",
    kvmServerCommand({
      program,
      args,
      cwd,
      allowAll,
      warmup,
      ephemeral,
      threads: 4,
    }),
    waitProgram,
    ["--disable-keepalive", "-c=4", `-z=${duration}`],
  );

  return group;
}

const httpserver = await addBenches(
  "rust httpserver",
  "./rust/target/release/httpserver",
);
const helloworld = await addBenches(
  "deno helloworld",
  "./deno/target/helloworld",
);
const renderer = await addBenches(
  "deno react renderer",
  "./deno/target/renderer",
);

console.log();
console.log(String(httpserver));
console.log();
console.log(String(helloworld));
console.log();
console.log(String(renderer));
