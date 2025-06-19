use tokio::io::AsyncRead;
use tokio::io::AsyncReadExt;
use tokio::io::AsyncWrite;
use tokio::io::AsyncWriteExt;
use tokio::net::TcpListener;
use tokio::net::UnixListener;

#[tokio::main(flavor = "current_thread")]
async fn main() -> Result<(), std::io::Error> {
    let addr = std::env::args()
        .nth(1)
        .unwrap_or_else(|| "127.0.0.1:8000".to_string());
    if addr.contains("/") {
        let server = UnixListener::bind(&addr)?;
        eprintln!("Listening on: {addr}");
        loop {
            let (stream, _) = server.accept().await?;
            tokio::spawn(async move {
                if let Err(e) = process(stream).await {
                    eprintln!("failed to process connection; error = {e}");
                }
            });
        }
    } else {
        let server = TcpListener::bind(&addr).await?;
        eprintln!("Listening on: {addr}");
        loop {
            let (stream, _) = server.accept().await?;
            tokio::spawn(async move {
                if let Err(e) = process(stream).await {
                    eprintln!("failed to process connection; error = {e}");
                }
            });
        }
    }
}

async fn process<Stream: AsyncRead + AsyncWrite + Unpin>(
    mut stream: Stream,
) -> Result<(), std::io::Error> {
    let mut req = [0; 4096];
    let _bytes_read = stream.read(&mut req).await?;
    if !req.starts_with(b"GET ") {
        return Err(std::io::Error::from(std::io::ErrorKind::InvalidData));
    }
    stream
        .write_all(
            b"HTTP/1.1 200 OK\r\n\
        Connection: close\r\n\
        Content-Type: text/plain; charset=utf-8\r\n\
        \r\n\
        Hello, World!",
        )
        .await?;
    stream.shutdown().await?;
    Ok(())
}
