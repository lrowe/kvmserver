#[link(name = "kvmserverguest", kind = "dylib")]
unsafe extern "C" {
    unsafe fn kvmserverguest_remote_resume(buffer: *mut u8, len: isize) -> isize;
    unsafe fn kvmserverguest_storage_wait_paused(bufferptr: *mut *mut u8, ret: isize) -> isize;
}

pub fn remote_resume(buffer: &mut [u8]) -> Result<&[u8], isize> {
    let len = unsafe { kvmserverguest_remote_resume(buffer.as_mut_ptr(), buffer.len() as isize) };
    if len < 0 {
        Err(len)
    } else {
        Ok(&buffer[0..len as usize])
    }
}

pub struct Storage {
    _private: (),
}

impl Storage {
    pub fn wait_paused(&mut self, return_value: isize) -> Result<Option<&mut [u8]>, isize> {
        let mut bufptr: *mut u8 = std::ptr::null_mut();
        let len = unsafe { kvmserverguest_storage_wait_paused(&mut bufptr, return_value) };
        if len < 0 {
            return Err(len);
        }
        if bufptr.is_null() {
            return Ok(None);
        }
        let buf = unsafe { std::slice::from_raw_parts_mut(bufptr, len.try_into().unwrap()) };
        Ok(Some(buf))
    }
}

// Limit storage to a single instance per thread but allow escaping from LocalKey::with.

use std::cell::Cell;
thread_local! {
    static STORAGE: Cell<Option<()>> = const { Cell::new(Some(())) };
}
pub fn get_storage() -> Option<Storage> {
    STORAGE
        .with(|cell| cell.take())
        .map(|_| Storage { _private: () })
}

impl Drop for Storage {
    fn drop(&mut self) {
        STORAGE.with(|cell| cell.set(Some(())));
    }
}
