import { DatabaseSync } from "node:sqlite";
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
  DENO_V8_FLAGS: "--predictable",
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
    program: "./target/httpserversync",
    allowAll,
  };
  Deno.test(
    "Deno compile httpserversync",
    testHelloWorld({ ...common }),
  );
  Deno.test(
    "Deno compile httpserversync ephemeral",
    testHelloWorld({ ...common, ephemeral }),
  );
  Deno.test(
    "Deno compile httpserversync ephemeral warmup",
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
    const text = await response.text();
    assert(response.ok, "response.ok");
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
    args: ["run", "--allow-all", "upstream.ts", "http://127.0.0.1:8001/"],
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
    async () => {
      await using _upstream = upstream();
      await testHelloWorld({ ...common })();
    },
  );
  Deno.test(
    "http-upstream ephemeral",
    async () => {
      await using _upstream = upstream();
      await testHelloWorld({ ...common, ephemeral })();
    },
  );
  Deno.test(
    "http-upstream ephemeral warmup",
    async () => {
      await using _upstream = upstream();
      await testHelloWorld({ ...common, ephemeral, warmup })();
    },
  );
}

{
  const common = {
    cwd,
    program: "deno",
    args: ["run", "--allow-all", "upstream.ts", "https://127.0.0.1:8001/"],
    env: {
      ...env,
      DENO_CERT: import.meta.dirname + "/target/ca.crt",
    },
    allowAll,
  };
  const cert = Deno.readTextFileSync(
    import.meta.dirname + "/target/server.crt",
  );
  const key = Deno.readTextFileSync(
    import.meta.dirname + "/target/server.key",
  );
  const upstream = () => {
    const server = Deno.serve(
      { port: 8001, cert, key },
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
    "https-upstream",
    async () => {
      await using _upstream = upstream();
      await testHelloWorld({ ...common })();
    },
  );
  Deno.test(
    "https-upstream ephemeral",
    async () => {
      await using _upstream = upstream();
      await testHelloWorld({ ...common, ephemeral })();
    },
  );
  Deno.test(
    "https-upstream ephemeral warmup",
    async () => {
      await using _upstream = upstream();
      await testHelloWorld({ ...common, ephemeral, warmup })();
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
    const bytes = await response.bytes();
    assert(response.ok, "response.ok");
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

{
  const makeTestDb = async () => {
    const tmpdir = await Deno.makeTempDir({ prefix: "denosqlite" });
    const path = `${tmpdir}/deno.sqlite`;
    const common = {
      cwd,
      program: "deno",
      args: ["run", "--allow-all", "sqlite.ts", path],
      env,
      allowAll,
    };
    const db = new DatabaseSync(path);
    // Does not work with WAL mode.
    //db.exec(`PRAGMA journal_mode = WAL;`);
    db.exec(`CREATE TABLE IF NOT EXISTS requests (url TEXT)`);
    return {
      db,
      common,
      assertCount(n: number) {
        const result = this.db.prepare(`SELECT count(*) FROM requests`).get();
        assertEquals(result?.["count(*)"], n, "assertCount");
      },
      async [Symbol.asyncDispose]() {
        this.db.close();
        await Deno.remove(tmpdir, { recursive: true });
      },
    };
  };
  const onResponse = async (response: Response) => {
    const json = await response.json();
    assert(response.ok, "response.ok");
    assertEquals(json.changes, 1);
  };
  Deno.test(
    "sqlite",
    async () => {
      await using testDb = await makeTestDb();
      await testHelloWorld({ ...testDb.common }, onResponse)();
      testDb.assertCount(1);
    },
  );
  Deno.test(
    "sqlite ephemeral",
    async () => {
      await using testDb = await makeTestDb();
      await testHelloWorld({ ...testDb.common, ephemeral }, onResponse)();
      testDb.assertCount(1);
    },
  );
  Deno.test(
    "sqlite ephemeral warmup",
    async () => {
      await using testDb = await makeTestDb();
      await testHelloWorld(
        { ...testDb.common, ephemeral, warmup },
        onResponse,
      )();
      testDb.assertCount(2);
    },
  );
}

{
  const common = {
    cwd,
    program: "deno",
    args: ["run", "--allow-all", "local.ts"],
    env,
    allowAll,
    storage: {
      program: "deno",
      args: ["run", "--allow-all", "remote.ts"],
      extra: ["--1-to-1"],
    },
  };
  Deno.test(
    "storage",
    testHelloWorld({ ...common }),
  );
  Deno.test(
    "storage ephemeral",
    testHelloWorld({ ...common, ephemeral }),
  );
  Deno.test(
    "storage ephemeral warmup",
    testHelloWorld({ ...common, ephemeral, warmup }),
  );
}
