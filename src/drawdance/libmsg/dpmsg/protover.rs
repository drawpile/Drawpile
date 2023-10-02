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

#[derive(Clone)]
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

    pub fn new(ns: &CStr, server: i32, major: i32, minor: i32) -> Self {
        ProtocolVersion {
            ns: ns.to_owned(),
            server,
            major,
            minor,
        }
    }

    pub fn new_major_minor(major: i32, minor: i32) -> Self {
        ProtocolVersion {
            ns: Self::current_namespace().to_owned(),
            server: Self::CURRENT_SERVER,
            major,
            minor,
        }
    }

    pub fn new_current() -> Self {
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

    pub fn is_valid(&self) -> bool {
        self.server >= 0 && self.major >= 0 && self.minor >= 0 && self.is_ns_valid()
    }

    fn is_ns_valid(&self) -> bool {
        let bytes = self.ns.to_bytes();
        bytes.len() > 0 && bytes.iter().all(|b| b.is_ascii_lowercase())
    }

    pub fn is_current(&self) -> bool {
        self.ns.as_bytes_with_nul() == DP_PROTOCOL_VERSION_NAMESPACE
            && self.server == Self::CURRENT_SERVER
            && self.major == Self::CURRENT_MAJOR
            && self.minor == Self::CURRENT_MINOR
    }

    pub fn is_future(&self) -> bool {
        self.ns.as_bytes_with_nul() == DP_PROTOCOL_VERSION_NAMESPACE
            && (self.server > Self::CURRENT_SERVER
                || (self.server == Self::CURRENT_SERVER
                    && (self.major > Self::CURRENT_MAJOR
                        || (self.major == Self::CURRENT_MAJOR
                            && self.minor > Self::CURRENT_MINOR))))
    }

    pub fn is_past_compatible(&self) -> bool {
        self.ns.as_bytes_with_nul() == DP_PROTOCOL_VERSION_NAMESPACE
            && self.server == 4
            && self.major == 21
            && self.minor == 2
    }

    pub fn should_have_system_id(&self) -> bool {
        self.ns.as_bytes_with_nul() == DP_PROTOCOL_VERSION_NAMESPACE
            && (self.server > 4
                || (self.server == 4 && (self.major > 24 || (self.major == 24 && self.minor >= 0))))
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

    pub fn equals(&self, other: &Self) -> bool {
        self.ns == other.ns
            && self.server == other.server
            && self.major == other.major
            && self.minor == other.minor
    }

    pub fn as_integer(&self) -> u64 {
        const MAX: u64 = (1u64 << 21u64) - 1u64;
        let a = (self.server.max(0i32) as u64).min(MAX);
        let b = (self.major.max(0i32) as u64).min(MAX);
        let c = (self.minor.max(0i32) as u64).min(MAX);
        (a << 42u64) | (b << 21u64) | c
    }
}

#[no_mangle]
pub extern "C" fn DP_protocol_version_new(
    ns: *const c_char,
    server: c_int,
    major: c_int,
    minor: c_int,
) -> *mut ProtocolVersion {
    Box::into_raw(Box::new(ProtocolVersion::new(
        unsafe { CStr::from_ptr(ns) },
        server,
        major,
        minor,
    )))
}

#[no_mangle]
pub extern "C" fn DP_protocol_version_new_major_minor(
    major: c_int,
    minor: c_int,
) -> *mut ProtocolVersion {
    Box::into_raw(Box::new(ProtocolVersion::new_major_minor(major, minor)))
}

#[no_mangle]
pub extern "C" fn DP_protocol_version_new_current() -> *mut ProtocolVersion {
    Box::into_raw(Box::new(ProtocolVersion::new_current()))
}

#[no_mangle]
pub extern "C" fn DP_protocol_version_new_clone(
    protover: *const ProtocolVersion,
) -> *mut ProtocolVersion {
    match unsafe { protover.as_ref() } {
        Some(p) => Box::into_raw(Box::new(p.clone())),
        None => null_mut(),
    }
}

#[no_mangle]
pub extern "C" fn DP_protocol_version_parse(s: *const c_char) -> *mut ProtocolVersion {
    if s.is_null() {
        null_mut()
    } else {
        match unsafe { CStr::from_ptr(s) }
            .to_str()
            .ok()
            .and_then(ProtocolVersion::parse)
        {
            Some(protover) => Box::into_raw(Box::new(protover)),
            None => null_mut(),
        }
    }
}

#[no_mangle]
pub extern "C" fn DP_protocol_version_free(protover: *mut ProtocolVersion) {
    if !protover.is_null() {
        drop(unsafe { Box::from_raw(protover) });
    }
}

#[no_mangle]
pub extern "C" fn DP_protocol_version_valid(protover: *const ProtocolVersion) -> bool {
    match unsafe { protover.as_ref() } {
        Some(p) => p.is_valid(),
        None => false,
    }
}

#[no_mangle]
pub extern "C" fn DP_protocol_version_is_current(protover: *const ProtocolVersion) -> bool {
    match unsafe { protover.as_ref() } {
        Some(p) => p.is_current(),
        None => false,
    }
}

#[no_mangle]
pub extern "C" fn DP_protocol_version_is_future(protover: *const ProtocolVersion) -> bool {
    match unsafe { protover.as_ref() } {
        Some(p) => p.is_future(),
        None => false,
    }
}

#[no_mangle]
pub extern "C" fn DP_protocol_version_is_past_compatible(protover: *const ProtocolVersion) -> bool {
    match unsafe { protover.as_ref() } {
        Some(p) => p.is_past_compatible(),
        None => false,
    }
}

#[no_mangle]
pub extern "C" fn DP_protocol_version_should_have_system_id(
    protover: *const ProtocolVersion,
) -> bool {
    match unsafe { protover.as_ref() } {
        Some(p) => p.should_have_system_id(),
        None => false,
    }
}

#[no_mangle]
pub extern "C" fn DP_protocol_version_ns(protover: *const ProtocolVersion) -> *const c_char {
    match unsafe { protover.as_ref() } {
        Some(p) => p.ns.as_ptr(),
        None => null(),
    }
}

#[no_mangle]
pub extern "C" fn DP_protocol_version_server(protover: *const ProtocolVersion) -> c_int {
    match unsafe { protover.as_ref() } {
        Some(p) => p.server,
        None => -1,
    }
}

#[no_mangle]
pub extern "C" fn DP_protocol_version_major(protover: *const ProtocolVersion) -> c_int {
    match unsafe { protover.as_ref() } {
        Some(p) => p.major,
        None => -1,
    }
}

#[no_mangle]
pub extern "C" fn DP_protocol_version_minor(protover: *const ProtocolVersion) -> c_int {
    match unsafe { protover.as_ref() } {
        Some(p) => p.minor,
        None => -1,
    }
}

#[no_mangle]
pub extern "C" fn DP_protocol_version_name(protover: *const ProtocolVersion) -> *const c_char {
    match unsafe { protover.as_ref() }
        .and_then(ProtocolVersion::version_name)
        .and_then(|bytes| CStr::from_bytes_with_nul(bytes).ok())
    {
        Some(s) => s.as_ptr(),
        None => null(),
    }
}

#[no_mangle]
pub extern "C" fn DP_protocol_version_client_compatibility(
    protover: *const ProtocolVersion,
) -> ProtocolCompatibility {
    match unsafe { protover.as_ref() } {
        Some(p) => p.client_compatibility(),
        None => ProtocolCompatibility::Incompatible,
    }
}

#[no_mangle]
pub extern "C" fn DP_protocol_version_equals(
    a: *const ProtocolVersion,
    b: *const ProtocolVersion,
) -> bool {
    if a == b {
        true
    } else if a.is_null() || b.is_null() {
        false
    } else {
        unsafe { &*a }.equals(unsafe { &*b })
    }
}

#[no_mangle]
pub extern "C" fn DP_protocol_version_as_integer(protover: *const ProtocolVersion) -> u64 {
    match unsafe { protover.as_ref() } {
        Some(p) => p.as_integer(),
        None => 0,
    }
}
