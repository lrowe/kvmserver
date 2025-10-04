fn main() {
    // See: https://github.com/rust-lang/cc-rs/issues/594
    let out_dir = std::env::var("OUT_DIR").unwrap();
    cc::Build::new()
        .get_compiler()
        .to_command()
        .args(["-shared", "-fPIC", "-o"])
        .arg(format!("{out_dir}/libkvmserverguest.so"))
        .arg("../../src/api/libkvmserverguest.c")
        .status()
        .unwrap();
    println!("cargo:rustc-link-search=native={out_dir}");
}
