mod adapters;
pub mod brushes_ffi;
mod brushpreview;
pub mod brushpreview_ffi;
pub mod messages_ffi;
pub mod paintengine_ffi;
mod snapshots;

use std::slice;
use tracing::{debug, error, info, warn, Level};
use tracing_subscriber;

#[no_mangle]
pub extern "C" fn rustpile_init() {
    tracing_subscriber::fmt::Subscriber::builder()
        .with_max_level(Level::DEBUG)
        .init();
}

#[no_mangle]
pub extern "C" fn qt_log_handler(loglevel: i32, message: *const u16, message_len: usize) {
    let msg = String::from_utf16_lossy(unsafe { slice::from_raw_parts(message, message_len) });

    match loglevel {
        0 => {
            error!("{}", msg)
        }
        1 => {
            warn!("{}", msg)
        }
        2 => {
            info!("{}", msg)
        }
        _ => {
            debug!("{}", msg)
        }
    }
}
