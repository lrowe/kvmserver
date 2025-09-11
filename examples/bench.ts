import * as echarts from "echarts";
import { assertEquals } from "@std/assert";
import { parseArgs } from "@std/cli/parse-args";
import { kvmServerCommand, waitForLine } from "./testutil.ts";

const flags = parseArgs(Deno.args, {
  string: ["group", "bench", "duration", "timeout", "warmup"],
  collect: ["group", "bench"],
  alias: { g: "group", b: "bench", t: "timeout", w: "warmup", z: "duration" },
  default: {
    duration: "1s",
    timeout: "5s",
    warmup: "1000",
  },
});

function parseTime(value: string): number {
  const m = value.trim().match(/^(\d+(?:\.\d+)?)(h|m|s|ms|us)$/);
  if (m) {
    const [_all, num, mag] = m;
    const magnitude = { h: 3600, m: 60, s: 1, ms: 0.001, us: 0.000001 }[mag] ??
      1;
    const result = Number(num) / magnitude;
    if (!Number.isNaN(result)) {
      return result;
    }
  }
  throw new Error(`Unknown time: ${value}`);
}

const path = "./bench.sock";
const args = [path];
const cwd = import.meta.dirname;
const ephemeral = true;
const allowAll = true;
const warmupRequests = Number(flags.warmup);
const duration = flags.duration;
const warmupRequestTimeout = parseTime(flags.timeout);

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

const warmup =
  (requestInit: RequestInit, n: number, path?: string) => async () => {
    using client = Deno.createHttpClient({
      proxy: path ? { transport: "unix", path } : undefined,
    });
    for (let i = 0; i < n; i++) {
      const controller = new AbortController();
      const id = warmupRequestTimeout
        ? setTimeout(
          () =>
            controller.abort(
              `Timeout: request ${i + 1} after ${flags.timeout}`,
            ),
          warmupRequestTimeout * 1000,
        )
        : undefined;
      try {
        const response = await fetch("http://127.0.0.1:8000/", {
          client,
          signal: controller.signal,
          ...requestInit,
        });
        await response.bytes();
        if (!response.ok) {
          throw new Error(
            `Warmup bad response: ${response.status} ${response.statusText}`,
          );
        }
      } finally {
        clearTimeout(id);
      }
    }
  };

async function oha(
  ...ohaArgs: string[]
): Promise<OhaResults> {
  const args = [
    ...ohaArgs,
    "--no-tui",
    "--output-format=json",
    "http://127.0.0.1:8000/",
  ];
  const command = new Deno.Command("oha", { args });
  console.log(["oha", ...args].join(" "));
  const output = await command.output();
  if (!output.success) {
    throw new Error(
      `oha status code: ${output.code}\n${
        new TextDecoder("utf-8").decode(output.stderr)
      }`,
    );
  }
  return JSON.parse(new TextDecoder("utf-8").decode(output.stdout));
}

const waitForLineStartsWith =
  (s: string, io: "stdout" | "stderr") => (proc: Deno.ChildProcess) =>
    Promise.race([
      waitForLine(proc[io], (line) => line.startsWith(s)),
      proc.status.then(({ code }) => {
        throw new Error(`Status code: ${code}`);
      }),
    ]);

const cleanupPath = () => Deno.remove(path).catch(() => {});

type NamedOhaResults = { name: string } & OhaResults;

class Bench {
  name: string;
  command: Deno.Command;
  ready: (proc: Deno.ChildProcess) => Promise<void>;
  ohaArgs: string[];
  warmupFn?: () => Promise<void>;
  cleanupFn?: () => Promise<void>;

  constructor(
    name: string,
    command: Deno.Command,
    ready: (proc: Deno.ChildProcess) => Promise<void>,
    ohaArgs: string[] = [],
    warmupFn?: () => Promise<void>,
    cleanupFn?: () => Promise<void>,
  ) {
    this.name = name;
    this.command = command;
    this.ready = ready;
    this.ohaArgs = ohaArgs;
    this.warmupFn = warmupFn;
    this.cleanupFn = cleanupFn;
  }
  async run(): Promise<NamedOhaResults> {
    try {
      await using proc = this.command.spawn();
      await this.ready(proc);
      await Promise.race([
        this.warmupFn?.(),
        proc.status.then(({ code }) => {
          throw new Error(`Status code: ${code}`);
        }),
      ]);
      const results = await oha(...this.ohaArgs);
      const { "aborted due to deadline": _ignore, ...errorDistribution } =
        results.errorDistribution;
      assertEquals(errorDistribution, {}, "errorDistribution");
      return { name: this.name, ...results };
    } finally {
      await this.cleanupFn?.();
    }
  }
}

class BenchGroup {
  title: string;
  benches: Bench[] = [];

  constructor(title: string) {
    this.title = title;
  }

  bench(
    name: string,
    command: Deno.Command,
    ready: (proc: Deno.ChildProcess) => Promise<void>,
    ohaArgs: string[] = [],
    warmupFn?: () => Promise<void>,
    cleanupFn: () => Promise<void> = cleanupPath,
  ) {
    this.benches.push(
      new Bench(name, command, ready, ohaArgs, warmupFn, cleanupFn),
    );
  }

  async run(filters: string[] = []): Promise<BenchGroupResult> {
    const rows = [];
    for (const bench of this.benches) {
      if (filters.length === 0 || filters.some((s) => bench.name.includes(s))) {
        console.log("BENCH: ", bench.name);
        const row = await bench.run();
        rows.push(row);
      }
    }
    return new BenchGroupResult(this.title, rows);
  }
}

class BenchGroupResult {
  title: string;
  rows: NamedOhaResults[];
  cols: Record<string, (row: NamedOhaResults) => string | number> = {
    name: (r) => r.name,
    average: (r) => Math.round(r.summary.average * 1_000_000),
    p50: (r) => Math.round(r.latencyPercentiles.p50 * 1_000_000),
    p90: (r) => Math.round(r.latencyPercentiles.p90 * 1_000_000),
    p99: (r) => Math.round(r.latencyPercentiles.p99 * 1_000_000),
  };
  format = (_name: string, value: string | number) =>
    typeof value === "number" ? `${value} µs` : value;

  constructor(title: string, rows: NamedOhaResults[]) {
    this.title = title;
    this.rows = rows;
  }

  getData(): Record<keyof BenchGroupResult["cols"], string | number>[] {
    return this.rows.map((r) =>
      Object.fromEntries(
        Object.entries(this.cols).map(([name, fn]) => [name, fn(r)]),
      )
    );
  }

  toChart() {
    const chart = echarts.init(null, null, {
      renderer: "svg",
      ssr: true,
      width: 600,
      height: 400,
    });
    chart.setOption({
      animation: false,
      title: { text: this.title, left: "center" },
      legend: { top: 30 },
      tooltip: {},
      dataset: {
        dimensions: Object.keys(this.cols),
        source: this.getData(),
      },
      xAxis: {
        type: "category",
        axisLabel: {
          interval: 0,
          formatter: (value: string) => value.replaceAll(" ", "\n"),
        },
      },
      yAxis: { name: "µs", nameLocation: "middle", nameGap: 40, nameRotate: 0 },
      series: Object.keys(this.cols).slice(1).map(() => ({ type: "bar" })),
    });
    const svg = chart.renderToSVGString();
    chart.dispose();
    return svg;
  }

  toString() {
    return [
      `### ${this.title}`,
      `| ${Object.keys(this.cols).join(" | ")} |`,
      `| ${Object.keys(this.cols).map(() => "---").join(" | ")} |`,
      ...this.getData().map((row) =>
        `| ${
          Object.keys(this.cols).map((k) => this.format(k, row[k])).join(" | ")
        } |`
      ),
    ].join("\n");
  }
}

function kvmserverBenches(title: string, program: string) {
  const group = new BenchGroup(title);

  group.bench(
    "native (reusing connection)",
    new Deno.Command(program, { args, cwd, stderr: "piped" }),
    waitForLineStartsWith("Listening", "stderr"),
    [`--unix-socket=${path}`, "-c=1", `-z=${duration}`],
    warmup({}, warmupRequests, path),
  );

  group.bench(
    "native",
    new Deno.Command(program, { args, cwd, stderr: "piped" }),
    waitForLineStartsWith("Listening", "stderr"),
    [`--unix-socket=${path}`, "--disable-keepalive", "-c=1", `-z=${duration}`],
    warmup({}, warmupRequests, path),
  );

  group.bench(
    "kvmserver threads=1 (reusing connection)",
    kvmServerCommand({ program, args, cwd, allowAll, warmup: warmupRequests }),
    waitForLineStartsWith("Program", "stdout"),
    [`--unix-socket=${path}`, "-c=1", `-z=${duration}`],
  );

  group.bench(
    "kvmserver threads=1",
    kvmServerCommand({ program, args, cwd, allowAll, warmup: warmupRequests }),
    waitForLineStartsWith("Program", "stdout"),
    [`--unix-socket=${path}`, "--disable-keepalive", "-c=1", `-z=${duration}`],
  );

  group.bench(
    "kvmserver ephemeral threads=1",
    kvmServerCommand({
      program,
      args,
      cwd,
      allowAll,
      warmup: warmupRequests,
      ephemeral,
    }),
    waitForLineStartsWith("Program", "stdout"),
    [`--unix-socket=${path}`, "--disable-keepalive", "-c=1", `-z=${duration}`],
  );

  group.bench(
    "kvmserver ephemeral threads=2",
    kvmServerCommand({
      program,
      args,
      cwd,
      allowAll,
      warmup: warmupRequests,
      ephemeral,
      threads: 2,
    }),
    waitForLineStartsWith("Program", "stdout"),
    [`--unix-socket=${path}`, "--disable-keepalive", "-c=2", `-z=${duration}`],
  );

  group.bench(
    "kvmserver ephemeral threads=4",
    kvmServerCommand({
      program,
      args,
      cwd,
      allowAll,
      warmup: warmupRequests,
      ephemeral,
      threads: 4,
    }),
    waitForLineStartsWith("Program", "stdout"),
    [`--unix-socket=${path}`, "--disable-keepalive", "-c=4", `-z=${duration}`],
  );

  group.bench(
    "kvmserver ephemeral threads=2 no-tail",
    kvmServerCommand({
      program,
      args,
      cwd,
      allowAll,
      warmup: warmupRequests,
      ephemeral,
      threads: 2,
    }),
    waitForLineStartsWith("Program", "stdout"),
    [`--unix-socket=${path}`, "--disable-keepalive", "-c=1", `-z=${duration}`],
  );

  return group;
}

function wasmtimeBenches(
  title: string,
  program: string,
  programFetchEvent?: string,
) {
  const group = new BenchGroup(title);
  for (
    const [wasmtime, name] of Object.entries({
      "wasmtime-reuse": "Wasmtime serve (experimental instance reuse on)",
      "wasmtime-noreuse": "Wasmtime serve (experimental instance reuse off)",
      "wasmtime": programFetchEvent
        ? "Wasmtime serve (release with fetch-event)"
        : "Wasmtime serve (release)",
    })
  ) {
    const args = [
      "serve",
      "--addr=127.0.0.1:8000",
      "-S",
      "common",
      wasmtime === "wasmtime" && programFetchEvent
        ? programFetchEvent
        : program,
    ];
    group.bench(
      name,
      new Deno.Command(wasmtime, { args, cwd, stderr: "piped" }),
      waitForLineStartsWith("Serving HTTP", "stderr"),
      ["-c=1", `-z=${duration}`],
      warmup({}, 100),
    );
  }
  return group;
}

function bunForkBenches(title: string, program: string) {
  const env = {
    BUN_GC_TIMER_DISABLE: "true",
    BUN_JSC_useConcurrentGC: "false",
    BUN_JSC_useConcurrentJIT: "false",
    BUN_PORT: "8000",
  };
  const group = new BenchGroup(title);
  group.bench(
    "Bun.serve (reusing connection)",
    new Deno.Command("bun", {
      args: [program, path],
      cwd,
      env,
      stdout: "piped",
    }),
    waitForLineStartsWith("Started", "stdout"),
    [`--unix-socket=${path}`, "-c=1", `-z=${duration}`],
    warmup({}, 100, path),
  );
  group.bench(
    "process forking",
    new Deno.Command("bun", {
      args: ["bun/fork.ts", program, path],
      cwd,
      env,
      stdout: "piped",
    }),
    waitForLineStartsWith("Started", "stdout"),
    [`--unix-socket=${path}`, "--disable-keepalive", "-c=1", `-z=${duration}`],
    warmup({ headers: { connection: "close" } }, 100, path),
  );
  return group;
}

function isolateBenches(title: string, program: string) {
  const group = new BenchGroup(title);
  for (
    const [server, name] of Object.entries({
      "node-isolated-vm/main.ts": "Main runtime",
      "node-isolated-vm/reused.ts": "Isolate reused",
      "node-isolated-vm/isolated.ts": "Isolate per request",
    })
  ) {
    const args = [server, program, "8000"];
    group.bench(
      name,
      new Deno.Command("node", { args, cwd, stdout: "piped" }),
      waitForLineStartsWith("Listening", "stdout"),
      ["-c=1", `-z=${duration}`],
      warmup({}, 100),
    );
  }
  return group;
}

const groups = [
  kvmserverBenches(
    "Rust minimal http server (async epoll)",
    "./rust/target/release/httpserver",
  ),
  kvmserverBenches(
    "Rust minimal http server (sync blocking)",
    "./rust/target/release/httpserversync",
  ),
  kvmserverBenches(
    "Deno helloworld",
    "./deno/target/helloworld",
  ),
  kvmserverBenches(
    "Deno React page rendering",
    "./deno/target/renderer",
  ),
  wasmtimeBenches(
    "Wasmtime helloworld.rs",
    "./wasmtime/ext/hello-wasi-http/target/wasm32-wasip2/release/hello_wasi_http.wasm",
  ),
  wasmtimeBenches(
    "Wasmtime helloworld.js",
    "./wasmtime/target/helloworld-incoming.wasm",
    "./wasmtime/target/helloworld.wasm",
  ),
  wasmtimeBenches(
    "Wasmtime React page rendering",
    "./wasmtime/target/renderer-incoming.wasm",
    "./wasmtime/target/renderer.wasm",
  ),
  bunForkBenches(
    "Bun process forking helloworld",
    "./bun/helloworld.js",
  ),
  bunForkBenches(
    "Bun process forking React page rendering",
    "./bun/target/renderer.mjs",
  ),
  isolateBenches(
    "Node isolated-vm helloworld",
    "node-isolated-vm/helloworld.cjs",
  ),
  isolateBenches(
    "Node isolated-vm React server rendering",
    "node-isolated-vm/target/renderer.js",
  ),
];

const results = [];
for (const group of groups) {
  if (
    flags.group.length === 0 || flags.group.some((s) => group.title.includes(s))
  ) {
    console.log("GROUP: ", group.title);
    const result = await group.run(flags.bench);
    results.push(result);
  }
}
const md = `\
![](./bench.svg)

${results.map(String).join("\n\n")}
`;
console.log(md);
Deno.mkdirSync("target", { recursive: true });
Deno.writeTextFileSync("target/bench.md", md);

for (const result of results) {
  if (result.title === "Deno React page rendering") {
    delete result.cols["average"];
    result.rows.splice(0, 1);
    result.rows.splice(1, 1);
    result.title += " native vs TinyKVM ephemeral VMs";
    const svg = result.toChart();
    Deno.writeTextFileSync("target/bench.svg", svg);
    break;
  }
}
