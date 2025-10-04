#[link(name = "kvmserverguest", kind = "dylib")]
unsafe extern "C" {
    pub unsafe fn remote_resume(buffer: *mut u8, len: isize) -> isize;
    pub unsafe fn storage_wait_paused(bufferptr: *mut *mut u8, ret: isize) -> isize;
}
