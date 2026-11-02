// Links libkokoro, built by CMake. CMake passes KOKORO_LIB_DIR (the directory holding kokoro.lib / libkokoro.so /
// libkokoro.dylib) and KOKORO_INSTALL_LIBDIR (CMAKE_INSTALL_LIBDIR, e.g. lib or lib64).
use std::env;

fn main() {
    println!("cargo:rerun-if-env-changed=KOKORO_LIB_DIR");
    println!("cargo:rerun-if-env-changed=KOKORO_INSTALL_LIBDIR");
    let lib_dir = env::var("KOKORO_LIB_DIR")
        .expect("KOKORO_LIB_DIR must name the directory containing the kokoro library (CMake sets it)");
    println!("cargo:rustc-link-search=native={lib_dir}");
    println!("cargo:rustc-link-lib=dylib=kokoro");
    emit_rpath(&lib_dir, &env::var("KOKORO_INSTALL_LIBDIR").unwrap_or_else(|_| "lib".to_owned()));
}

// Build tree: the executable sits next to libkokoro. Install tree: bin/ and <libdir>/ are siblings. Test binaries stay
// in the cargo target directory, so they point at lib_dir itself.
fn emit_rpath(lib_dir: &str, install_libdir: &str) {
    let origin = match env::var("CARGO_CFG_TARGET_OS").as_deref() {
        Ok("macos") => "@loader_path",
        Ok("windows") | Err(_) => return,
        Ok(_) => "$ORIGIN",
    };
    for path in [origin.to_owned(), format!("{origin}/../{install_libdir}")] {
        println!("cargo:rustc-link-arg-bins=-Wl,-rpath,{path}");
        println!("cargo:rustc-link-arg-tests=-Wl,-rpath,{path}");
    }
    println!("cargo:rustc-link-arg-tests=-Wl,-rpath,{lib_dir}");
}
