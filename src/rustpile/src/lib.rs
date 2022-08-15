// lint: allow unsafe pointer dereference since this is a FFI crate
#![allow(
    clippy::not_unsafe_ptr_arg_deref,
    clippy::missing_safety_doc,
    clippy::unusual_byte_groupings
)]

mod adapters;
pub mod blendmode_ffi;
pub mod brushes_ffi;
mod brushpreview;
pub mod brushpreview_ffi;
pub mod logging;
pub mod messages_ffi;
pub mod paintengine_ffi;
mod recindex;
mod snapshots;
