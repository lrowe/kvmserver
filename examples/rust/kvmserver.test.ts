import { testHelloWorld } from "../testutil.ts";

const common = {
  cwd: import.meta.dirname,
  allowAll: true,
};
const ephemeral = true;
const warmup = 1;

{
  const program = "./target/release/httpserver";
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

{
  const program = "./target/release/httpserversync";
  Deno.test(
    "httpserversync",
    testHelloWorld({ ...common, program }),
  );
  Deno.test(
    "httpserversync ephemeral",
    testHelloWorld({ ...common, program, ephemeral }),
  );
  Deno.test(
    "httpserversync ephemeral warmup",
    testHelloWorld({ ...common, program, ephemeral, warmup }),
  );
}
