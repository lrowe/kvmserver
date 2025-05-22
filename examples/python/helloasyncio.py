import asyncio


async def handle_client(reader, writer):
    _request = await reader.readuntil(b"\r\n\r\n")
    writer.write(
        "\r\n".join(
            [
                "HTTP/1.1 200 OK",
                "Connection: close",
                "Content-Type: text/plain; charset=utf-8",
                "",
                "Hello, World!",
            ]
        ).encode("utf-8")
    )
    writer.write_eof()
    await writer.drain()
    writer.close()
    await writer.wait_closed()


async def main(host="127.0.0.1", port=8000):
    server = await asyncio.start_server(handle_client, host, port)
    addrs = ", ".join(str(sock.getsockname()) for sock in server.sockets)
    print(f"Serving on {addrs}")
    async with server:
        await server.serve_forever()


if __name__ == "__main__":
    asyncio.run(main())
