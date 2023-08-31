// SPDX-License-Identifier: MIT
use std::env;
use std::path::PathBuf;

fn main() {
    println!("cargo:rerun-if-changed=wrapper.h");

    let bindings = bindgen::Builder::default()
        .header("wrapper.h")
        .clang_arg("-I../bundled")
        .clang_arg("-I../libcommon")
        .clang_arg("-I../libmsg")
        .clang_arg("-I../libengine")
        .allowlist_function("(DP|json)_.*")
        .allowlist_type("(DP|JSON)_.*")
        .allowlist_var("DP_.*")
        .parse_callbacks(Box::new(bindgen::CargoCallbacks))
        .prepend_enum_name(false)
        .generate()
        .expect("Couldn't generate bindings for drawdance");

    let out_path = PathBuf::from(env::var("OUT_DIR").unwrap());
    bindings
        .write_to_file(out_path.join("bindings.rs"))
        .expect("Couldn't write bindings for drawdance");
}
