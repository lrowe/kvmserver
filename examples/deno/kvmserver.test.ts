import { assertEquals } from "@std/assert";
import { testHelloWorld } from "../testutil.ts";

const cwd = import.meta.dirname;
const allowAll = true;
const ephemeral = true;
const warmup = 1;

{
  const common = {
    cwd,
    program: "deno",
    args: [
      "run",
      "--allow-all",
      'data:,Deno.serve(() => new Response("Hello, World!"))',
    ],
    env: {
      // Could not initialize cache database '$HOME/.cache/deno/dep_analysis_cache_v2', deleting and retrying...
      // (SqliteFailure(Error { code: FileLockingProtocolFailed, extended_code: 15 }, Some("locking protocol")))
      DENO_DIR: ".cache/deno",
      DENO_V8_FLAGS: "--single-threaded",
    },
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
    "Denocompile helloworld ephemeral warmup",
    testHelloWorld({ ...common, ephemeral, warmup }),
  );
}

{
  const common = {
    cwd,
    program: "./target/renderer",
    allowAll,
  };
  Deno.test(
    "Deno compile renderer",
    testHelloWorld({ ...common }, (text) => {
      assertEquals(text.length, 31099);
    }),
  );
  Deno.test(
    "Deno compile renderer ephemeral",
    testHelloWorld({ ...common, ephemeral }, (text) => {
      assertEquals(text.length, 31099);
    }),
  );
  Deno.test(
    "Denocompile renderer ephemeral warmup",
    testHelloWorld({ ...common, ephemeral, warmup }, (text) => {
      assertEquals(text.length, 31099);
    }),
  );
}
