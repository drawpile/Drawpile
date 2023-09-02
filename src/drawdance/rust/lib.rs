// SPDX-License-Identifier: GPL-3.0-or-later
#![allow(non_upper_case_globals)]
#![allow(non_camel_case_types)]
#![allow(non_snake_case)]

use std::ffi::{c_char, CStr};

include!(concat!(env!("OUT_DIR"), "/bindings.rs"));

extern "C" {
    pub fn DP_cpu_support_init();
    pub fn DP_cmake_config_version() -> *const c_char;
}

pub mod engine;
pub mod msg;

pub fn init() {
    unsafe { DP_cpu_support_init() };
}

fn dp_error() -> String {
    unsafe { CStr::from_ptr(DP_error()) }
        .to_str()
        .unwrap_or_default()
        .to_owned()
}

pub fn dp_cmake_config_version() -> String {
    unsafe { CStr::from_ptr(DP_cmake_config_version()) }
        .to_str()
        .unwrap_or_default()
        .to_owned()
}
