# See https://docs.python.org/3/library/wsgiref.html#examples


def wsgi_app(environ, start_response):
    status = "200 OK"
    headers = [("Content-type", "text/plain; charset=utf-8")]
    start_response(status, headers)
    return [b"Hello, World!"]


if __name__ == "__main__":
    from wsgiref.simple_server import make_server

    with make_server("", 8000, wsgi_app) as httpd:
        print("Serving on port 8000...")
        httpd.serve_forever()
