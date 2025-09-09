// The built in fetch event handling is not compatible with instance reuse.
// To avoud requiring a bundler this file is concatenated to a module with a
// fetch `handler` in scope. Adapted from:
// https://github.com/bytecodealliance/jco/tree/main/examples/components/http-hello-world

import {
  Fields,
  OutgoingBody,
  OutgoingResponse,
  ResponseOutparam,
} from "wasi:http/types@0.2.3";

export const incomingHandler = {
  async handle(_incomingRequest, responseOutparam) {
    const outgoingResponse = new OutgoingResponse(new Fields());
    const outgoingBody = outgoingResponse.body();
    {
      const outputStream = outgoingBody.write();
      const response = await handler(new Request("http://localhost/"));
      const bytes = await response.arrayBuffer();
      outgoingResponse.setStatusCode(response.status);
      outputStream.write(bytes);
      outputStream[Symbol.dispose]();
    }
    OutgoingBody.finish(outgoingBody, undefined);
    ResponseOutparam.set(responseOutparam, {
      tag: "ok",
      val: outgoingResponse,
    });
  },
};
