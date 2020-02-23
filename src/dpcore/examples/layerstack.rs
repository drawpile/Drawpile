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

use dpcore::paint::layerstack::{LayerFill, LayerInsertion, LayerStack};
use dpcore::paint::tile::Tile;
use dpcore::paint::{editlayer, Blendmode, BrushMask, Color, Layer};

mod utils;

fn brush_stroke(layer: &mut Layer, y: i32, color: &Color) {
    for x in (10..246).step_by(5) {
        let w = 16 + ((x as f32 / 40.0 * 3.14).sin() * 15.0) as i32;
        let brush = BrushMask::new_round_pixel(w as u32, 0.4);
        editlayer::draw_brush_dab(
            layer,
            0,
            x - w / 2,
            y - w / 2,
            &brush,
            &color,
            Blendmode::Normal,
        );
    }
}

fn main() {
    let mut layerstack = LayerStack::new(256, 256);
    layerstack.background = Tile::new_solid(&Color::rgb8(255, 255, 255), 0);
    layerstack.add_layer(1, LayerFill::Solid(Color::TRANSPARENT), LayerInsertion::Top);
    layerstack.add_layer(2, LayerFill::Solid(Color::TRANSPARENT), LayerInsertion::Top);

    layerstack.get_layer_mut(2).unwrap().opacity = 0.5;

    brush_stroke(
        layerstack.get_layer_mut(1).unwrap(),
        60,
        &Color::rgb8(255, 0, 0),
    );
    brush_stroke(
        layerstack.get_layer_mut(2).unwrap(),
        80,
        &Color::rgb8(0, 255, 0),
    );

    utils::save_layerstack(&layerstack, "example_layerstack.png");
}
