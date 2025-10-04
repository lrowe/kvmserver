use kvmserver_examples_rust::storage_wait_paused;

fn main() {
    let mut ret: isize = 0;
    loop {
        let mut bufptr: *mut u8 = std::ptr::null_mut();
        let len = unsafe { storage_wait_paused(&mut bufptr, ret) };
        if bufptr.is_null() || len < 0 {
            ret = -1;
            continue;
        }
        let buf = unsafe { std::slice::from_raw_parts_mut(bufptr as *mut u8, len as usize) };
        let message = b"Hello from Rust storage inside TinyKVM";
        if message.len() > buf.len() {
            ret = -1;
            continue;
        }
        buf.copy_from_slice(message);
        ret = message.len().try_into().unwrap();
    }
}
