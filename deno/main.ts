import { connect } from "jsr:@db/redis";

console.log("Hello from Deno inside TinyKVM");

Deno.serve({
	port: 8000,
}, async (req) => {
  const url = new URL(req.url);
  const path = url.pathname;

  if (path === "/") {
	return new Response("Hello from Deno!");
  } else if (path === "/redis") {
	const client = await connect({
	  hostname: "127.0.0.1",
	  port: 6379,
	});
	const value = await client.get("key");
	return new Response(`Value from Redis: ${value}`);
  } else {
	return new Response("Not Found", { status: 404 });
  }
})

export default {
	fetch: async (req: Request) => {
		const url = new URL(req.url);
		const path = url.pathname;

		if (path === "/") {
			return new Response("Hello from Deno!");
		} else if (path === "/redis") {
			const client = await connect({
				hostname: "127.0.0.1",
				port: 6379,
			});
			const value = await client.get("key");
			return new Response(`Value from Redis: ${value}`);
		} else {
			return new Response("Not Found", { status: 404 });
		}
	}
}
