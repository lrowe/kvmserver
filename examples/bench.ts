import * as echarts from "echarts";
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
  ...ohaArgs: string[]
): Promise<OhaResults> {
  const args = [
    ...ohaArgs,
    "--wait-ongoing-requests-after-deadline",
    "--no-tui",
    "--output-format=json",
    "http://127.0.0.1:8000/",
  ];
  const command = new Deno.Command("oha", { args });
  console.log(["oha", ...args].join(" "));
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

function waitServingHTTP(proc: Deno.ChildProcess) {
  return Promise.race([
    waitForLine(proc.stderr, (line) => line.startsWith("Serving HTTP")),
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
  title: string;
  rows: NamedOhaResults[] = [];
  cols: Record<string, (row: NamedOhaResults) => string | number> = {
    name: (r) => r.name,
    average: (r) => Math.round(r.summary.average * 1_000_000),
    p50: (r) => Math.round(r.latencyPercentiles.p50 * 1_000_000),
    p90: (r) => Math.round(r.latencyPercentiles.p90 * 1_000_000),
    p99: (r) => Math.round(r.latencyPercentiles.p99 * 1_000_000),
  };
  format = (_name: string, value: string | number) =>
    typeof value === "number" ? `${value} µs` : value;

  constructor(title: string) {
    this.title = title;
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
      const results = await oha(...ohaArgs);
      assertEquals(results.errorDistribution, {}, "errorDistribution");
      this.rows.push({ name, ...results });
    } finally {
      await Deno.remove(path).catch(() => {});
    }
  }

  getData(): Record<keyof BenchGroup["cols"], string | number>[] {
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

async function addBenches(title: string, program: string) {
  const group = new BenchGroup(title);

  await group.bench(
    "native (reusing connection)",
    new Deno.Command(program, { args, cwd, stderr: "piped" }),
    waitListening,
    [`--unix-socket=${path}`, "-c=1", `-z=${duration}`],
    async () => {
      await oha(`--unix-socket=${path}`, "-c=1", `-n=${warmup}`);
    },
  );

  await group.bench(
    "native",
    new Deno.Command(program, { args, cwd, stderr: "piped" }),
    waitListening,
    [`--unix-socket=${path}`, "--disable-keepalive", "-c=1", `-z=${duration}`],
    async () => {
      await oha(`--unix-socket=${path}`, "-c=1", `-n=${warmup}`);
    },
  );

  await group.bench(
    "kvmserver threads=1",
    kvmServerCommand({ program, args, cwd, allowAll, warmup }),
    waitProgram,
    [`--unix-socket=${path}`, "--disable-keepalive", "-c=1", `-z=${duration}`],
  );

  await group.bench(
    "kvmserver ephemeral threads=1",
    kvmServerCommand({ program, args, cwd, allowAll, warmup, ephemeral }),
    waitProgram,
    [`--unix-socket=${path}`, "--disable-keepalive", "-c=1", `-z=${duration}`],
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
    [`--unix-socket=${path}`, "--disable-keepalive", "-c=2", `-z=${duration}`],
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
    [`--unix-socket=${path}`, "--disable-keepalive", "-c=4", `-z=${duration}`],
  );

  await group.bench(
    "kvmserver ephemeral threads=2 no-tail",
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
    [`--unix-socket=${path}`, "--disable-keepalive", "-c=1", `-z=${duration}`],
  );

  return group;
}

async function addWasmBenches(
  title: string,
  program_name: Record<string, string>,
) {
  const group = new BenchGroup(title);
  for (const [program, name] of Object.entries(program_name)) {
    const args = ["serve", "--addr=127.0.0.1:8000", "-S", "common", program];
    await group.bench(
      name,
      new Deno.Command("wasmtime", { args, cwd, stderr: "piped" }),
      waitServingHTTP,
      ["-c=1", `-z=${duration}`],
      async () => {
        await oha("-c=1", "-n=100");
      },
    );
  }
  return group;
}

const httpserver = await addBenches(
  "Rust minimal http server",
  "./rust/target/release/httpserver",
);
const helloworld = await addBenches(
  "Deno helloworld",
  "./deno/target/helloworld",
);
const renderer = await addBenches(
  "Deno React page rendering",
  "./deno/target/renderer",
);
const wasmtime = await addWasmBenches("Wasmtime ComponentJS StarlingMonkey", {
  "./wasmtime/target/helloworld.wasm": "helloworld",
  "./wasmtime/target/helloworldaot.wasm": "helloworld aot",
  "./wasmtime/target/renderer.wasm": "React page rendering",
});

const md = `\
![](./bench.svg)

${httpserver}

${helloworld}

${renderer}

${wasmtime}
`;
console.log(md);

delete renderer.cols["average"];
renderer.rows.splice(0, 1);
renderer.rows.splice(1, 1);
renderer.title += " native vs TinyKVM ephemeral VMs";
const svg = renderer.toChart();
await Deno.mkdir("target", { recursive: true });
await Deno.writeTextFile("target/bench.svg", svg);
await Deno.writeTextFile("target/bench.md", md);
