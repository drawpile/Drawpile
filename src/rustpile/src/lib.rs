mod brushpreview;
pub mod brushpreview_ffi;

#[no_mangle]
pub extern "C" fn rustpile_init() {
    println!("Rustpile init!");
}
