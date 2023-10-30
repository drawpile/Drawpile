use std::slice::{from_raw_parts, from_raw_parts_mut};

// SPDX-License-Identifier: GPL-3.0-or-later
use crate::{DP_UPixel8, DP_free};

impl DP_UPixel8 {
    pub fn b(self) -> u8 {
        ((unsafe { self.color }) & 0xff_u32) as u8
    }

    pub fn g(self) -> u8 {
        ((unsafe { self.color } >> 8_u32) & 0xff_u32) as u8
    }

    pub fn r(self) -> u8 {
        ((unsafe { self.color } >> 16_u32) & 0xff_u32) as u8
    }

    pub fn a(self) -> u8 {
        ((unsafe { self.color } >> 24_u32) & 0xff_u32) as u8
    }
}

pub struct UPixels8 {
    data: *mut DP_UPixel8,
    len: usize,
}

impl UPixels8 {
    pub fn new(data: *mut DP_UPixel8, len: usize) -> UPixels8 {
        Self {
            data,
            len: if data.is_null() { 0 } else { len },
        }
    }

    pub fn as_slice(&self) -> &[DP_UPixel8] {
        unsafe { from_raw_parts(self.data, self.len) }
    }

    pub fn as_mut_slice(&mut self) -> &mut [DP_UPixel8] {
        unsafe { from_raw_parts_mut(self.data, self.len) }
    }
}

impl Drop for UPixels8 {
    fn drop(&mut self) {
        unsafe { DP_free(self.data.cast()) }
    }
}
