// SPDX-License-Identifier: GPL-3.0-or-later
use drawdance::{
    DP_TransientLayerProps, DP_byte_order, DP_transient_layer_props_title_set, DP_BIG_ENDIAN,
    DP_LITTLE_ENDIAN,
};
use std::slice::from_raw_parts;

fn strlen16(s: *const u16) -> usize {
    let mut i = 0usize;
    loop {
        if unsafe { *s.add(i) } == 0 {
            return i;
        } else {
            i += 1;
        }
    }
}

fn is_little_endian() -> bool {
    match unsafe { DP_byte_order() } {
        DP_LITTLE_ENDIAN => true,
        DP_BIG_ENDIAN => false,
        x => panic!("Invalid byte order {:?}", x),
    }
}

fn decode_utf16be(tlp: *mut DP_TransientLayerProps, be: &[u16]) {
    let str = if is_little_endian() {
        let le: Vec<u16> = be.iter().map(|c| c.to_le()).collect();
        String::from_utf16_lossy(&le)
    } else {
        String::from_utf16_lossy(be)
    };
    let bytes = str.as_bytes();
    unsafe { DP_transient_layer_props_title_set(tlp, bytes.as_ptr().cast(), bytes.len()) };
}

#[no_mangle]
pub extern "C" fn DP_psd_read_utf16be_layer_title(
    tlp: *mut DP_TransientLayerProps,
    be: *const u16,
) -> bool {
    if be.is_null() {
        false
    } else {
        decode_utf16be(tlp, unsafe { from_raw_parts(be, strlen16(be)) });
        true
    }
}
