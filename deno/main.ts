import { connect } from "jsr:@db/redis";

console.log("Hello from deno inside TinyKVM");

Deno.serve({
	port: 8080,
	reusePort: true,
}, async (req) => {
  const url = new URL(req.url);
  const path = url.pathname;

  if (path === "/") {
	return new Response("Hello from Deno!");
  } else if (path === "/redis") {
	const client = await connect({
	  hostname: "localhost",
	  port: 6379,
	});
	const value = await client.get("key");
	return new Response(`Value from Redis: ${value}`);
  } else {
	return new Response("Not Found", { status: 404 });
  }
})