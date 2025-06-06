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

export class KvmServerCommand {
  #command: Deno.Command;
  constructor(options: KvmServerCommandOptions) {
    const { cwd, env } = options;
    const args = [
      "--output=L",
      KVMSERVER,
      ...options.threads !== undefined
        ? ["--threads", String(options.threads)]
        : [],
      ...options.warmup !== undefined
        ? ["--warmup", String(options.warmup)]
        : [],
      ...options.allowAll ? ["--allow-all"] : [],
      ...options.ephemeral ? ["--ephemeral"] : [],
      "--",
      options.program,
      ...options.args ?? [],
    ];
    console.log(`stdbuf ${args.map(quote).join(" ")}`);
    this.#command = new Deno.Command("stdbuf", {
      args,
      cwd,
      env,
      stdout: "piped",
    });
  }

  async spawn(): Promise<Deno.ChildProcess> {
    const { promise, resolve } = Promise.withResolvers<void>();
    const proc = this.#command.spawn();
    const status = proc.status.then((status) => {
      if (!status.success) {
        throw new Error(`Status code: ${status.code}`);
      }
    });
    try {
      proc.stdout
        .pipeThrough(new TextDecoderStream("latin1"))
        .pipeThrough(new TextLineStream())
        .pipeThrough(
          new TransformStream({
            transform(line, controller) {
              console.log(line);
              controller.enqueue(line);
              if (line.startsWith("Program")) {
                resolve();
              }
            },
          }),
        )
        .pipeTo(new WritableStream());
      await Promise.race([promise, status]);
    } catch (error) {
      await proc[Symbol.asyncDispose]();
      throw error;
    }
    return proc;
  }
}

export function testHelloWorld(options: KvmServerCommandOptions) {
  return async () => {
    const command = new KvmServerCommand(options);
    await using _proc = await command.spawn();
    using client = Deno.createHttpClient({ poolMaxIdlePerHost: 0 });
    const response = await fetch("http://127.0.0.1:8000/", { client });
    assert(response.ok, "response.ok");
    const text = await response.text();
    assertEquals(text, "Hello, World!");
  };
}
