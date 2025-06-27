import { assert, assertEquals } from "@std/assert";
import { TextLineStream } from "@std/streams/text-line-stream";

function quote(s: string) {
  // From: https://github.com/nodejs/node/issues/34840#issuecomment-677402567
  if (s === "") return `''`;
  if (!/[^%+,-.\/:=@_0-9A-Za-z]/.test(s)) return s;
  return `'` + s.replace(/'/g, `'"'`) + `'`;
}

export const KVMSERVER = Deno.env.get("KVMSERVER") ??
  new URL(import.meta.resolve("../.build/kvmserver")).pathname;

type KvmServerCommandOptions = {
  program: string;
  args?: string[];
  cwd?: Deno.CommandOptions["cwd"];
  env?: Deno.CommandOptions["env"];
  ephemeral?: boolean;
  allowAll?: boolean;
  warmup?: number;
  threads?: number;
};

export function kvmServerCommand(
  options: KvmServerCommandOptions,
): Deno.Command {
  const { cwd, env } = options;
  const args = [
    "--output=L",
    KVMSERVER,
    ...options.threads !== undefined
      ? ["--threads", String(options.threads)]
      : [],
    ...options.warmup !== undefined ? ["--warmup", String(options.warmup)] : [],
    ...options.allowAll ? ["--allow-all"] : [],
    ...options.ephemeral ? ["--ephemeral"] : [],
    "--",
    options.program,
    ...options.args ?? [],
  ];
  console.log(`stdbuf ${args.map(quote).join(" ")}`);
  return new Deno.Command("stdbuf", {
    args,
    cwd,
    env,
    stdout: "piped",
  });
}

export function waitForLine(
  stream: ReadableStream<Uint8Array<ArrayBuffer>>,
  callbackFn: (line: string) => boolean,
): Promise<void> {
  const { promise, resolve } = Promise.withResolvers<void>();
  stream
    .pipeThrough(new TextDecoderStream("latin1"))
    .pipeThrough(new TextLineStream())
    .pipeThrough(
      new TransformStream({
        transform(line, controller) {
          console.log(line);
          controller.enqueue(line);
          if (callbackFn(line)) {
            resolve();
          }
        },
      }),
    )
    .pipeTo(new WritableStream());
  return promise;
}

export function testHelloWorld(
  options: KvmServerCommandOptions,
  onResponse: (response: Response) => Promise<void> = async (response) => {
    assert(response.ok, "response.ok");
    const text = await response.text();
    assertEquals(text, "Hello, World!");
  },
) {
  return async () => {
    const command = kvmServerCommand(options);
    await using proc = command.spawn();
    await Promise.race([
      waitForLine(proc.stdout, (line) => line.startsWith("Program")),
      proc.status.then(({ code }) => {
        throw new Error(`Status code: ${code}`);
      }),
    ]);
    using client = Deno.createHttpClient({ poolMaxIdlePerHost: 0 });
    const response = await fetch("http://127.0.0.1:8000/", { client });
    await onResponse(response);
  };
}
