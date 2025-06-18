import { testHelloWorld } from "../testutil.ts";

const common = {
  program: "deno",
  cwd: import.meta.dirname,
  env: {
    // Could not initialize cache database '$HOME/.cache/deno/dep_analysis_cache_v2', deleting and retrying...
    // (SqliteFailure(Error { code: FileLockingProtocolFailed, extended_code: 15 }, Some("locking protocol")))
    DENO_DIR: ".cache/deno",
    DENO_V8_FLAGS: "--single-threaded",
  },
  allowAll: true,
};
const ephemeral = true;
const warmup = 1;

{
  const args = [
    "run",
    "--allow-all",
    'data:,Deno.serve(() => new Response("Hello, World!"))',
  ];
  Deno.test(
    "Deno.serve",
    testHelloWorld({ ...common, args }),
  );
  Deno.test(
    "Deno.serve ephemeral",
    testHelloWorld({ ...common, args, ephemeral }),
  );
  Deno.test(
    "Deno.serve ephemeral warmup",
    testHelloWorld({ ...common, args, ephemeral, warmup }),
  );
}
