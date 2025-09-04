function handler(_req) {
  return new Response("Hello, World!");
}


export default { fetch: handler };
