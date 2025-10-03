import { assertEquals } from "@std/assert";
import { KVMSERVER } from "../testutil.ts";

Deno.test(
  "clock test",
  async () => {
    const command = new Deno.Command(KVMSERVER, {
      args: ["--allow-read=/lib", "run", "./target/test"],
      cwd: import.meta.dirname,
    });
    const result = await command.output();
    assertEquals(result.code, 0, "code");
    const stdout = new TextDecoder("latin1").decode(result.stdout);
    console.log(stdout);
    const lines = stdout.trim().split("\n");
    assertEquals(
      lines.at(-1),
      "Realtime clock is monotonic and did not go backwards.",
    );
  },
);
