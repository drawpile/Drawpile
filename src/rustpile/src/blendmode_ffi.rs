// This file is part of Drawpile.
// Copyright (C) 2021 Calle Laakkonen
//
// Drawpile is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// As additional permission under section 7, you are allowed to distribute
// the software through an app store, even if that store has restrictive
// terms and conditions that are incompatible with the GPL, provided that
// the source is also available under the GPL with or without this permission
// through a channel without those restrictive terms and conditions.
//
// Drawpile is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Drawpile.  If not, see <https://www.gnu.org/licenses/>.

use dpcore::paint::Blendmode;
use std::slice;

/// Find a blending mode matching the given SVG name
///
/// If no match is found, the default (normal) mode is returned.
#[no_mangle]
pub extern "C" fn blendmode_from_svgname(name: *const u16, name_len: usize) -> Blendmode {
    let name = String::from_utf16_lossy(unsafe { slice::from_raw_parts(name, name_len) });

    return Blendmode::from_svg_name(&name).unwrap_or_default();
}

#[no_mangle]
pub extern "C" fn blendmode_svgname(mode: Blendmode, name_len: &mut usize) -> *const u8 {
    let name = mode.svg_name();
    *name_len = name.len();
    name.as_ptr()
}
