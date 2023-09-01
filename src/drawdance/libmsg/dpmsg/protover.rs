// SPDX-License-Identifier: GPL-3.0-or-later

use drawdance::{
    DP_PROTOCOL_VERSION_MAJOR, DP_PROTOCOL_VERSION_MINOR, DP_PROTOCOL_VERSION_NAMESPACE,
    DP_PROTOCOL_VERSION_SERVER,
};
use once_cell::sync::Lazy;
use regex::Regex;
use std::{
    ffi::{c_char, c_int, CStr, CString},
    ptr::{null, null_mut},
};

pub struct ProtocolVersion {
    pub ns: CString,
    pub server: i32,
    pub major: i32,
    pub minor: i32,
}

#[repr(C)]
pub enum ProtocolCompatibility {
    Compatible,
    MinorIncompatibility,
    BackwardCompatible,
    Incompatible,
}

impl ProtocolVersion {
    const CURRENT_SERVER: i32 = DP_PROTOCOL_VERSION_SERVER as i32;
    const CURRENT_MAJOR: i32 = DP_PROTOCOL_VERSION_MAJOR as i32;
    const CURRENT_MINOR: i32 = DP_PROTOCOL_VERSION_MINOR as i32;

    pub fn current() -> Self {
        ProtocolVersion {
            ns: Self::current_namespace().to_owned(),
            server: Self::CURRENT_SERVER,
            major: Self::CURRENT_MAJOR,
            minor: Self::CURRENT_MINOR,
        }
    }

    pub fn parse(s: &str) -> Option<Self> {
        static RE: Lazy<Regex> =
            Lazy::new(|| Regex::new(r"\A([a-z]+):([0-9]+)\.([0-9]+)\.([0-9]+)\z").unwrap());
        let m = RE.captures(s)?;
        let (_, [ns, server, major, minor]) = m.extract();
        Some(ProtocolVersion {
            ns: CString::new(ns).ok()?,
            server: server.parse().ok()?,
            major: major.parse().ok()?,
            minor: minor.parse().ok()?,
        })
    }

    pub fn version_name(&self) -> Option<&'static [u8]> {
        if self.ns.as_ref() == Self::current_namespace() && self.server == 4 {
            if self.major == 24 {
                Some(b"2.2.x\0")
            } else if self.major == 21 || self.major == 22 {
                Some(b"2.2.0 beta\0")
            } else if self.major == 21 && self.minor == 2 {
                Some(b"2.1.x\0")
            } else if self.major == 20 && self.minor == 1 {
                Some(b"2.0.x\0")
            } else {
                None
            }
        } else {
            None
        }
    }

    pub fn client_compatibility(&self) -> ProtocolCompatibility {
        if self.ns.as_ref() == Self::current_namespace() {
            if self.major == Self::CURRENT_MAJOR {
                if self.minor == Self::CURRENT_MINOR {
                    ProtocolCompatibility::Compatible
                } else {
                    ProtocolCompatibility::MinorIncompatibility
                }
            } else if self.major == 21 && self.minor == 2 {
                ProtocolCompatibility::BackwardCompatible // Drawpile 2.1 recording
            } else {
                ProtocolCompatibility::Incompatible
            }
        } else {
            ProtocolCompatibility::Incompatible
        }
    }

    fn current_namespace() -> &'static CStr {
        CStr::from_bytes_with_nul(DP_PROTOCOL_VERSION_NAMESPACE).unwrap()
    }
}

#[no_mangle]
pub extern "C" fn DP_protocol_version_parse(s: *const c_char) -> *mut ProtocolVersion {
    match unsafe { CStr::from_ptr(s) }
        .to_str()
        .ok()
        .and_then(ProtocolVersion::parse)
    {
        Some(protover) => Box::into_raw(Box::new(protover)),
        None => null_mut(),
    }
}

#[no_mangle]
pub extern "C" fn DP_protocol_version_free(protover: *mut ProtocolVersion) {
    if !protover.is_null() {
        drop(unsafe { Box::from_raw(protover) });
    }
}

#[no_mangle]
pub extern "C" fn DP_protocol_version_ns(protover: &mut ProtocolVersion) -> *const c_char {
    protover.ns.as_ptr()
}

#[no_mangle]
pub extern "C" fn DP_protocol_version_server(protover: &mut ProtocolVersion) -> c_int {
    protover.server
}

#[no_mangle]
pub extern "C" fn DP_protocol_version_major(protover: &mut ProtocolVersion) -> c_int {
    protover.major
}

#[no_mangle]
pub extern "C" fn DP_protocol_version_minor(protover: &mut ProtocolVersion) -> c_int {
    protover.minor
}

#[no_mangle]
pub extern "C" fn DP_protocol_version_name(protover: &mut ProtocolVersion) -> *const c_char {
    match protover
        .version_name()
        .and_then(|bytes| CStr::from_bytes_with_nul(bytes).ok())
    {
        Some(s) => s.as_ptr(),
        None => null(),
    }
}

#[no_mangle]
pub extern "C" fn DP_protocol_version_client_compatibility(
    protover: &mut ProtocolVersion,
) -> ProtocolCompatibility {
    protover.client_compatibility()
}
