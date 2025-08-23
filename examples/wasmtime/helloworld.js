function handler(_request) {
  return new Response("Hello, World!");
}

addEventListener("fetch", (event) => event.respondWith(handler(event.request)));
