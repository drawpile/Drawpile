mod adapters;
mod brushpreview;
pub mod brushpreview_ffi;
pub mod paintengine_ffi;

use tracing::Level;
use tracing_subscriber;

#[no_mangle]
pub extern "C" fn rustpile_init() {
    tracing_subscriber::fmt::Subscriber::builder()
        .with_max_level(Level::DEBUG)
        .init();
}
