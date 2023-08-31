// SPDX-License-Identifier: GPL-3.0-or-later
#![allow(non_upper_case_globals)]
#![allow(non_camel_case_types)]
#![allow(non_snake_case)]

include!(concat!(env!("OUT_DIR"), "/bindings.rs"));

extern "C" {
    pub fn DP_cmake_config_version() -> *const ::std::os::raw::c_char;
}

pub mod engine;
pub mod msg;

fn dp_error() -> String {
    unsafe { std::ffi::CStr::from_ptr(DP_error()) }
        .to_str()
        .unwrap()
        .to_owned()
}

pub fn dp_cmake_config_version() -> String {
    unsafe { std::ffi::CStr::from_ptr(DP_cmake_config_version()) }
        .to_str()
        .unwrap()
        .to_owned()
}
