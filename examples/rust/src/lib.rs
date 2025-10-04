use std::ffi::c_void;

#[link(name = "kvmserverguest", kind = "dylib")]
unsafe extern "C" {
    pub unsafe fn remote_resume(buffer: *mut c_void, len: usize);
    pub unsafe fn storage_wait_paused() -> *mut c_void;
}
