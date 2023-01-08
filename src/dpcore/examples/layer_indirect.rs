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

use dpcore::paint::{editlayer, BitmapLayer, Blendmode, BrushMask, Color, Tile, BIT15_F32};
use std::f32::consts::PI;

mod utils;

fn brush_stroke(layer: &mut BitmapLayer, y: i32) {
    let black = Color {
        r: 0.0,
        g: 0.0,
        b: 0.0,
        a: 1.0,
    };

    for x in (10..246).step_by(5) {
        let w = 16 + ((x as f32 / 40.0 * PI).sin() * 15.0) as i32;
        let brush = BrushMask::new_round_pixel(w as u32);
        editlayer::draw_brush_dab(
            layer,
            0,
            x - w / 2,
            y - w / 2,
            &brush,
            &black,
            Blendmode::Normal,
            (0.4 * BIT15_F32) as u16,
        );
    }
}

fn main() {
    let mut layer = BitmapLayer::new(0, 256, 256, Tile::new(&Color::rgb8(255, 255, 255), 0));

    // The brush stroke drawn in direct mode
    brush_stroke(&mut layer, 60);

    // Indirect mode using a sublayer
    let sublayer = layer.get_or_create_sublayer(1);
    sublayer.metadata_mut().opacity = 0.5;
    brush_stroke(sublayer, 120);
    editlayer::merge_sublayer(&mut layer, 1);

    utils::save_layer(&layer, "example_layer_indirect.png");
}
