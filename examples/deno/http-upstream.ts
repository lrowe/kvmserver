// deno run --allow-net 'data:,Deno.serve({ port: 8001}, ()=>new Response("hello"))';
const URL = "http://127.0.0.1:8001/";

Deno.serve(async () => {
  const response = await fetch(URL);
  const body = await response.text();
  return new Response(body);
});
