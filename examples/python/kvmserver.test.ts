import { testHelloWorld } from "../testutil.ts";

const common = {
  cwd: import.meta.dirname,
  program: ".venv/bin/python3",
  allowAll: true,
};
const ephemeral = true;
const warmup = 1;

{
  const args = ["hellowsgi.py"];
  Deno.test(
    "wsgiref",
    testHelloWorld({ ...common, args }),
  );
  Deno.test(
    "wsgiref ephemeral",
    testHelloWorld({ ...common, args, ephemeral }),
  );
  Deno.test.ignore(
    "wsgiref ephemeral warmup",
    testHelloWorld({ ...common, args, ephemeral, warmup }),
  );
}

{
  const args = ["helloasyncio.py"];
  Deno.test(
    "asyncio",
    testHelloWorld({ ...common, args }),
  );
  Deno.test(
    "asyncio ephemeral",
    testHelloWorld({ ...common, args, ephemeral }),
  );
  Deno.test(
    "asyncio ephemeral warmup",
    testHelloWorld({ ...common, args, ephemeral, warmup }),
  );
}

{
  const args = ["-m", "uvicorn", "--interface", "wsgi", "hellowsgi:wsgi_app"];
  Deno.test(
    "uvicorn wsgi",
    testHelloWorld({ ...common, args }),
  );
  Deno.test(
    "uvicorn wsgi ephemeral",
    testHelloWorld({ ...common, args, ephemeral }),
  );
  Deno.test(
    "uvicorn wsgi ephemeral warmup",
    testHelloWorld({ ...common, args, ephemeral, warmup }),
  );
}

{
  const args = ["-m", "uvicorn", "--interface", "asgi3", "helloasgi:asgi_app"];
  Deno.test(
    "uvicorn asgi",
    testHelloWorld({ ...common, args }),
  );
  Deno.test(
    "uvicorn asgi ephemeral",
    testHelloWorld({ ...common, args, ephemeral }),
  );
  Deno.test(
    "uvicorn asgi ephemeral warmup",
    testHelloWorld({ ...common, args, ephemeral, warmup }),
  );
}
