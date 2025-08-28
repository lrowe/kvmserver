use std::io::Read;
use std::io::Write;
use std::net::Shutdown;
use std::net::TcpListener;
use std::os::unix::net::UnixListener;


fn main() -> Result<(), std::io::Error> {
    let addr = std::env::args()
        .nth(1)
        .unwrap_or_else(|| "127.0.0.1:8000".to_string());
    if addr.contains("/") {
        let server = UnixListener::bind(&addr)?;
        eprintln!("Listening on: {addr}");
        loop {
            let (mut stream, _) = server.accept().expect("accept error");
            if let Err(e) = process(&mut stream) {
                eprintln!("failed to process connection; error = {e}");
            }
            stream.shutdown(Shutdown::Both)?;
        }
    } else {
        let server = TcpListener::bind(&addr)?;
        eprintln!("Listening on: {addr}");
        loop {
            let (mut stream, _) = server.accept().expect("accept error");
            if let Err(e) = process(&mut stream) {
                eprintln!("failed to process connection; error = {e}");
            }
            stream.shutdown(Shutdown::Both)?;
        }
    }
}

fn process<Stream: Read + Write>(stream: &mut Stream) -> Result<(), std::io::Error> {
    let mut req = [0; 4096];
    let _bytes_read = stream.read(&mut req)?;
    if !req.starts_with(b"GET ") {
        return Err(std::io::Error::from(std::io::ErrorKind::InvalidData));
    }
    stream.write_all(
        b"HTTP/1.1 200 OK\r\n\
        Connection: close\r\n\
        Content-Type: text/plain; charset=utf-8\r\n\
        \r\n\
        Hello, World!",
    )?;
    Ok(())
}
