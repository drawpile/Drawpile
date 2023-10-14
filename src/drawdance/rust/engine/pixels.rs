// SPDX-License-Identifier: GPL-3.0-or-later
use crate::DP_UPixel8;

impl DP_UPixel8 {
    pub fn b(self) -> u8 {
        ((unsafe { self.color }) & 0xffu32) as u8
    }

    pub fn g(self) -> u8 {
        ((unsafe { self.color } >> 8u32) & 0xffu32) as u8
    }

    pub fn r(self) -> u8 {
        ((unsafe { self.color } >> 16u32) & 0xffu32) as u8
    }

    pub fn a(self) -> u8 {
        ((unsafe { self.color } >> 24u32) & 0xffu32) as u8
    }
}
