import { testHelloWorld } from "../testutil.ts";

const common = {
  cwd: import.meta.dirname,
  allowAll: true,
};
const ephemeral = true;
const warmup = 1;

{
  const program = "./target/debug/httpserver";
  Deno.test(
    "httpserver",
    testHelloWorld({ ...common, program }),
  );
  Deno.test(
    "httpserver ephemeral",
    testHelloWorld({ ...common, program, ephemeral }),
  );
  Deno.test(
    "httpserver ephemeral warmup",
    testHelloWorld({ ...common, program, ephemeral, warmup }),
  );
}
