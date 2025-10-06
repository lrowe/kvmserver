import { testHelloWorld } from "../testutil.ts";

const common = {
  program: "lune",
  cwd: import.meta.dirname,
  allowAll: true,
  extra: [],
};
const ephemeral = true;
const warmup = 1;

{
  const args = ["run", "./test.luau"];
  Deno.test(
    "net.serve",
    testHelloWorld({ ...common, args }),
  );
  Deno.test(
    "net.serve ephemeral",
    testHelloWorld({ ...common, args, ephemeral }),
  );
  Deno.test(
    "net.serve ephemeral warmup",
    testHelloWorld({ ...common, args, ephemeral, warmup }),
  );
}
