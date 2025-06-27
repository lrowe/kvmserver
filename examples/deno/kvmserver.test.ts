import { assert, assertEquals } from "@std/assert";
import { testHelloWorld } from "../testutil.ts";

const cwd = import.meta.dirname;
const allowAll = true;
const ephemeral = true;
const warmup = 1;
const env = {
  // Could not initialize cache database '$HOME/.cache/deno/dep_analysis_cache_v2', deleting and retrying...
  // (SqliteFailure(Error { code: FileLockingProtocolFailed, extended_code: 15 }, Some("locking protocol")))
  DENO_DIR: ".cache/deno",
  DENO_V8_FLAGS: "--single-threaded",
};

{
  const common = {
    cwd,
    program: "deno",
    args: [
      "run",
      "--allow-all",
      'data:,Deno.serve(() => new Response("Hello, World!"))',
    ],
    env,
    allowAll,
  };
  Deno.test(
    "Deno.serve",
    testHelloWorld({ ...common }),
  );
  Deno.test(
    "Deno.serve ephemeral",
    testHelloWorld({ ...common, ephemeral }),
  );
  Deno.test(
    "Deno.serve ephemeral warmup",
    testHelloWorld({ ...common, ephemeral, warmup }),
  );
}

{
  const common = {
    cwd,
    program: "./target/helloworld",
    allowAll,
  };
  Deno.test(
    "Deno compile helloworld",
    testHelloWorld({ ...common }),
  );
  Deno.test(
    "Deno compile helloworld ephemeral",
    testHelloWorld({ ...common, ephemeral }),
  );
  Deno.test(
    "Deno compile helloworld ephemeral warmup",
    testHelloWorld({ ...common, ephemeral, warmup }),
  );
}

{
  const common = {
    cwd,
    program: "./target/renderer",
    allowAll,
  };
  const onResponse = async (response: Response) => {
    assert(response.ok, "response.ok");
    const text = await response.text();
    assertEquals(text.length, 31099);
  };
  Deno.test(
    "Deno compile renderer",
    testHelloWorld({ ...common }, onResponse),
  );
  Deno.test(
    "Deno compile renderer ephemeral",
    testHelloWorld({ ...common, ephemeral }, onResponse),
  );
  Deno.test(
    "Deno compile renderer ephemeral warmup",
    testHelloWorld({ ...common, ephemeral, warmup }, onResponse),
  );
}

{
  const common = {
    cwd,
    program: "deno",
    args: ["run", "--allow-all", "http-upstream.ts"],
    env,
    allowAll,
  };
  const upstream = () => {
    const server = Deno.serve(
      { port: 8001 },
      () => new Response("Hello, World!"),
    );
    return {
      [Symbol.asyncDispose]: () => {
        server.shutdown();
        return server.finished;
      },
    };
  };
  Deno.test(
    "http-upstream",
    async (t) => {
      await using _upstream = upstream();
      await t.step(
        "basic",
        testHelloWorld({ ...common }),
      );
      await t.step(
        "ephemeral",
        testHelloWorld({ ...common, ephemeral }),
      );
      await t.step(
        "ephemeral warmup",
        testHelloWorld({ ...common, ephemeral, warmup }),
      );
    },
  );
}

{
  const common = {
    cwd,
    program: "./target/imagemagick",
    allowAll,
  };
  const onResponse = async (response: Response) => {
    assert(response.ok, "response.ok");
    const bytes = await response.bytes();
    assertEquals(bytes.length, 2385160);
  };
  Deno.test(
    "imagemagick",
    testHelloWorld({ ...common }, onResponse),
  );
  Deno.test(
    "imagemagick ephemeral",
    testHelloWorld({ ...common, ephemeral }, onResponse),
  );
  Deno.test(
    "imagemagick ephemeral warmup",
    testHelloWorld({ ...common, ephemeral, warmup }, onResponse),
  );
}
