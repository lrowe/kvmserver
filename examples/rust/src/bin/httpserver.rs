use httparse;
use std::io::Error;
use std::io::ErrorKind;
use tokio::io::AsyncRead;
use tokio::io::AsyncReadExt;
use tokio::io::AsyncWrite;
use tokio::io::AsyncWriteExt;
use tokio::net::TcpListener;
use tokio::net::UnixListener;

#[tokio::main(flavor = "current_thread")]
async fn main() -> Result<(), Error> {
    let addr = std::env::args()
        .nth(1)
        .unwrap_or_else(|| "127.0.0.1:8000".to_string());
    if addr.contains("/") {
        let listener = UnixListener::bind(&addr)?;
        eprintln!("Listening on: {addr}");
        loop {
            let (stream, _) = listener.accept().await?;
            tokio::spawn(async move {
                if let Err(e) = process(stream).await {
                    eprintln!("failed to process connection; error = {e}");
                }
            });
        }
    } else {
        let listener = TcpListener::bind(&addr).await?;
        eprintln!("Listening on: {addr}");
        loop {
            let (stream, _) = listener.accept().await?;
            tokio::spawn(async move {
                if let Err(e) = process(stream).await {
                    eprintln!("failed to process connection; error = {e}");
                }
            });
        }
    }
}

// // If full http parsing is not requrired this will suffice:
// async fn process<Stream: AsyncRead + AsyncWrite + Unpin>(mut stream: Stream) -> Result<(), Error> {
//     let mut req = [0; 4096];
//     let _bytes_read = stream.read(&mut req).await?;
//     if !req.starts_with(b"GET ") {
//         return Err(Error::from(ErrorKind::InvalidData));
//     }
//     stream
//         .write_all(
//             b"HTTP/1.1 200 OK\r\n\
//         Connection: close\r\n\
//         Content-Type: text/plain; charset=utf-8\r\n\
//         \r\n\
//         Hello, World!",
//         )
//         .await?;
//     stream.shutdown().await?;
//     Ok(())
// }

async fn process<Stream: AsyncRead + AsyncWrite + Unpin>(mut stream: Stream) -> Result<(), Error> {
    let mut offset = 0;
    let mut buf = [0; 4096];
    loop {
        loop {
            let bytes_read = stream.read(&mut buf[offset..]).await?;
            if bytes_read == 0 {
                return Err(Error::from(ErrorKind::UnexpectedEof));
            }
            offset += bytes_read;
            let mut headers = [httparse::EMPTY_HEADER; 16];
            let mut req: httparse::Request<'_, '_> = httparse::Request::new(&mut headers);
            match req.parse(&buf[..offset]) {
                Ok(httparse::Status::Complete(bytes_consumed)) => {
                    let version = req.version.unwrap();
                    let mut close = version == 0;
                    if req.method != Some("GET") {
                        stream
                            .write_all(
                                b"HTTP/1.1 405 Method Not Allowed\r\n\
                            Connection: close\r\n\
                            Content-Type: text/plain; charset=utf-8\r\n\
                            \r\n\
                            Method Not Allowed",
                            )
                            .await?;
                        return Err(Error::from(ErrorKind::InvalidData));
                    }
                    for header in &*req.headers {
                        if header.name.eq_ignore_ascii_case("connection") {
                            close = !header.value.eq_ignore_ascii_case("keep-alive".as_bytes());
                        }
                    }
                    let body = "Hello, World!";
                    let conn = if close { "close" } else { "keep_alive" };
                    let length = body.as_bytes().len();
                    stream
                        .write_all(
                            format!(
                                "HTTP/1.1 200 OK\r\n\
                        Connection: {conn}\r\n\
                        Content-Length: {length}\r\n\
                        Content-Type: text/plain; charset=utf-8\r\n\
                        \r\n\
                        {body}"
                            )
                            .as_bytes(),
                        )
                        .await?;
                    if close {
                        return Ok(());
                    }
                    buf.copy_within(bytes_consumed..offset, 0);
                    offset -= bytes_consumed;
                    break;
                }
                Ok(httparse::Status::Partial) => {
                    if offset == buf.len() {
                        stream
                            .write_all(
                                b"HTTP/1.1 400 Bad Request\r\n\
                            Connection: close\r\n\
                            Content-Type: text/plain; charset=utf-8\r\n\
                            \r\n\
                            Bad Request",
                            )
                            .await?;
                        return Err(Error::from(ErrorKind::InvalidData));
                    }
                    continue;
                }
                Err(error) => {
                    return Err(Error::new(ErrorKind::InvalidData, error));
                }
            }
        }
    }
}
