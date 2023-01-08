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

use dpcore::paint::{
    editlayer, BitmapLayer, Blendmode, BrushMask, Color, Rectangle, Tile, BIT15_U16,
};

mod utils;

fn main() {
    let mut layer = BitmapLayer::new(0, 256, 512, Tile::Blank);

    // Draw a nice background
    let colors = [
        0xff845ec2, 0xffd65db1, 0xffff6f91, 0xffff9671, 0xffffc75f, 0xfff9f871,
    ];

    for (i, &c) in colors.iter().enumerate() {
        editlayer::fill_rect(
            &mut layer,
            0,
            &Color::from_argb32(c),
            Blendmode::Normal,
            &Rectangle::new(i as i32 * 40, 0, 40, 512),
        );
    }

    // Draw dabs using all the blend modes
    let modes = [
        Blendmode::Erase,
        Blendmode::Normal,
        Blendmode::Multiply,
        Blendmode::Divide,
        Blendmode::Burn,
        Blendmode::Dodge,
        Blendmode::Darken,
        Blendmode::Lighten,
        Blendmode::Subtract,
        Blendmode::Add,
        Blendmode::Recolor,
        Blendmode::Behind,
        Blendmode::ColorErase,
        Blendmode::Screen,
        Blendmode::NormalAndEraser,
        Blendmode::LuminosityShineSai,
        Blendmode::Overlay,
        Blendmode::HardLight,
        Blendmode::SoftLight,
        Blendmode::LinearBurn,
        Blendmode::LinearLight,
        Blendmode::Hue,
        Blendmode::Saturation,
        Blendmode::Luminosity,
        Blendmode::Color,
        Blendmode::Replace,
    ];

    let brush = BrushMask::new_round_pixel(10);
    let dabcolor = Color::rgb8(255, 0, 0);

    for (i, &mode) in modes.iter().enumerate() {
        println!("Mode: {:?}", mode);
        for x in (10..250).step_by(8) {
            editlayer::draw_brush_dab(
                &mut layer,
                0,
                x,
                10 + i as i32 * 15,
                &brush,
                &dabcolor,
                mode,
                BIT15_U16,
            );
        }
    }
    utils::save_layer(&layer, "example_layer_blend_dabs.png");
}
