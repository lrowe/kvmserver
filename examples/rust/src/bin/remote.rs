use kvmserver_examples_rust::storage_wait_paused;
use std::process::ExitCode;
const BUF_SIZE: usize = 256;

fn main() -> ExitCode {
    loop {
        let bufptr = unsafe { storage_wait_paused() };
        if bufptr.is_null() {
            return ExitCode::FAILURE;
        }
        let buf = unsafe { std::slice::from_raw_parts_mut(bufptr as *mut u8, BUF_SIZE) };
        buf.copy_from_slice(b"Hello from Rust storage inside TinyKVM\0");
    }
}
