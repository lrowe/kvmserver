use std::io::Error;
use std::io::ErrorKind;
use std::io::Read;
use std::io::Write;
use std::net::Shutdown;
use std::net::TcpListener;
use std::os::unix::net::UnixListener;

use kvmserver_examples_rust::remote_resume;

fn main() -> Result<(), Error> {
    let addr = std::env::args()
        .nth(1)
        .unwrap_or_else(|| "127.0.0.1:8000".to_string());
    if addr.contains("/") {
        let listener = UnixListener::bind(&addr)?;
        eprintln!("Listening on: {addr}");
        loop {
            let (mut stream, _) = listener.accept()?;
            if let Err(e) = process(&mut stream) {
                eprintln!("failed to process connection; error = {e}");
            }
            stream.shutdown(Shutdown::Write).unwrap_or_default();
        }
    } else {
        let listener = TcpListener::bind(&addr)?;
        eprintln!("Listening on: {addr}");
        loop {
            let (mut stream, _) = listener.accept()?;
            if let Err(e) = process(&mut stream) {
                eprintln!("failed to process connection; error = {e}");
            }
            stream.shutdown(Shutdown::Write).unwrap_or_default();
        }
    }
}

fn process<Stream: Read + Write>(stream: &mut Stream) -> Result<(), Error> {
    let mut req = [0; 4096];
    let _bytes_read = stream.read(&mut req)?;
    if !req.starts_with(b"GET ") {
        return Err(Error::from(ErrorKind::InvalidData));
    }
    let mut buf = [0u8; 256];
    let len = unsafe { remote_resume(buf.as_mut_ptr(), buf.len() as isize) };
    if len < 0 || len as usize > buf.len() {
        return Err(Error::from(ErrorKind::InvalidData));
    }
    let message = &buf[0..len as usize];
    stream.write_all(
        &[
            b"HTTP/1.1 200 OK\r\n\
            Connection: close\r\n\
            Content-Type: text/plain; charset=utf-8\r\n\
            \r\n\
            Hello from Rust inside TinyKVM\n",
            message,
            b"\n",
        ]
        .concat(),
    )?;
    Ok(())
}
