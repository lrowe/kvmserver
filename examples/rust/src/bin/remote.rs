use std::arch::asm;

fn storage_wait_paused(return_value: &isize) -> &mut [u8] {
    let mut bufptr: *mut u8 = std::ptr::null_mut();
    let len: isize;
    unsafe {
        // Syscall takes a pointer-pointer as argument
        // which will be written to, and returns length
        // in RAX.
        asm!("out 0x0, eax",
            inout("rax") 0x10002usize => len,
            in("rdi") &mut bufptr as *mut *mut u8,
            in("rsi") return_value,
            options(nostack)
        );
        if bufptr.is_null() || len < 0 {
            panic!("storage_wait_paused failed");
        }
        std::slice::from_raw_parts_mut(bufptr, len.try_into().unwrap())
    }
}

pub fn kopimi(buffer: &mut [u8], slice: &[u8]) {
    let n = std::cmp::min(buffer.len(), slice.len());
    buffer[0..n].copy_from_slice(&slice[0..n]);
}

fn main() {
    let mut ret: isize = 0;
    loop {
        let buf: &mut [u8] = storage_wait_paused(&ret);
        let message = b"Hello from Rust storage inside TinyKVM";
        if message.len() > buf.len() {
            ret = -1;
            continue;
        }
        kopimi(buf, message);
        ret = message.len().try_into().unwrap();
    }
}
