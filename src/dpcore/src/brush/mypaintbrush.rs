// This file is part of Drawpile.
// Copyright (C) 2022 askmeaboutloom
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

use crate::paint::Color;

#[derive(Clone, Copy)]
#[repr(C)]
pub struct ControlPoints {
    pub xvalues: [f32; 64],
    pub yvalues: [f32; 64],
    pub n: i32,
}

#[derive(Clone, Copy)]
#[repr(C)]
pub struct MyPaintMapping {
    pub base_value: f32,
    // This 18 must be hard-coded because of cbindgen limitations. It's really
    // referring to the value of MYPAINT_BRUSH_INPUTS_COUNT. There's a static
    // assertion in the C++ code to make sure that these match up.
    pub inputs: [ControlPoints; 18usize],
}

// Settings are split off from the brush because they're huge and blow up the
// stack if allocated there and then cloned a bunch. So instead we keep them
// as separate, heap-allocated objects and only pass pointers to them around.
#[derive(Clone, Copy)]
#[repr(C)]
pub struct MyPaintSettings {
    // The 64 must also be hard-coded because of cbindgen. This one's referring
    // to MYPAINT_BRUSH_SETTINGS_COUNT.
    pub mappings: [MyPaintMapping; 64usize],
}

#[derive(Clone, Copy)]
#[repr(C)]
pub struct MyPaintBrush {
    // Color is only stored as HSV in the brush, we track the RGB separately.
    pub color: Color,
    // Forces locked alpha on all dabs, acting as Recolor mode.
    pub lock_alpha: bool,
    // Forces transparent color on all dabs, acting as Erase mode.
    pub erase: bool,
}
