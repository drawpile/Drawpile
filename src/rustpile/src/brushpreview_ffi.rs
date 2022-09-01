// This file is part of Drawpile.
// Copyright (C) 2020 Calle Laakkonen
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

use super::brushpreview::{BrushPreview, BrushPreviewShape};
use dpcore::brush::{ClassicBrush, MyPaintBrush, MyPaintSettings};
use dpcore::paint::color::{pixels15_to_8, ZERO_PIXEL8};
use dpcore::paint::tile::{TILE_LENGTH, TILE_SIZEI};
use dpcore::paint::{AoE, Color, FlattenedTileIterator, LayerViewOptions};

use core::ffi::c_void;

#[no_mangle]
pub unsafe extern "C" fn brushpreview_new(width: u32, height: u32) -> *mut BrushPreview {
    Box::into_raw(Box::new(BrushPreview::new(width, height)))
}

#[no_mangle]
pub unsafe extern "C" fn brushpreview_free(bp: *mut BrushPreview) {
    if !bp.is_null() {
        drop(Box::from_raw(bp))
    }
}

#[no_mangle]
pub extern "C" fn brushpreview_render_classic(
    bp: &mut BrushPreview,
    brush: &ClassicBrush,
    shape: BrushPreviewShape,
) -> i32 {
    bp.render_classic(brush, shape) as i32
}

#[no_mangle]
pub extern "C" fn brushpreview_render_mypaint(
    bp: &mut BrushPreview,
    brush: &MyPaintBrush,
    settings: &MyPaintSettings,
    shape: BrushPreviewShape,
) -> i32 {
    bp.render_mypaint(brush, settings, shape) as i32
}

#[no_mangle]
pub extern "C" fn brushpreview_floodfill(
    bp: &mut BrushPreview,
    color: &Color,
    tolerance: f32,
    expansion: i32,
    fill_under: bool,
) {
    bp.floodfill(*color, tolerance, expansion, fill_under);
}

#[no_mangle]
pub extern "C" fn brushpreview_paint(
    bp: &BrushPreview,
    ctx: *mut c_void,
    paint_func: extern "C" fn(ctx: *mut c_void, x: i32, y: i32, pixels: *const u8),
) {
    let opts = LayerViewOptions::default();
    let mut pixels = [ZERO_PIXEL8; TILE_LENGTH];
    FlattenedTileIterator::new(&bp.layerstack, &opts, AoE::Everything).for_each(|(i, j, t)| {
        pixels15_to_8(&mut pixels, &t.pixels);
        paint_func(
            ctx,
            i * TILE_SIZEI,
            j * TILE_SIZEI,
            pixels.as_ptr() as *const u8,
        )
    });
}
