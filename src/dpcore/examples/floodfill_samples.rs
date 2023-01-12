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

use std::env;

use dpcore::paint::floodfill::{expand_floodfill, floodfill};
use dpcore::paint::{editlayer, Blendmode, Color, LayerID, LayerInsertion, LayerStack, Rectangle};

mod utils;

fn main() {
    let mut expansion = 0;
    let args: Vec<String> = env::args().collect();
    if args.len() > 1 {
        expansion = args[1].parse::<i32>().unwrap();
    }

    let mut layerstack = LayerStack::new(768 * 2, 832);

    // Set up the canvas
    layerstack
        .root_mut()
        .add_bitmap_layer(1, Color::TRANSPARENT, LayerInsertion::Top);

    layerstack
        .root_mut()
        .add_bitmap_layer(2, Color::TRANSPARENT, LayerInsertion::Top);

    let (bgimage, w, h) = utils::load_image_data(include_bytes!("testdata/filltest.png"));

    editlayer::draw_image8(
        layerstack.root_mut().get_bitmaplayer_mut(1).unwrap(),
        0,
        &bgimage,
        &Rectangle::new(0, 0, w, h),
        1.0,
        Blendmode::Replace,
    );
    editlayer::draw_image8(
        layerstack.root_mut().get_bitmaplayer_mut(1).unwrap(),
        0,
        &bgimage,
        &Rectangle::new(768, 0, w, h),
        1.0,
        Blendmode::Replace,
    );

    // Test on-layer floodfill
    do_floodfill(
        &mut layerstack,
        86,
        86,
        Color {
            r: 1.0,
            g: 0.0,
            b: 0.0,
            a: 1.0,
        },
        1,
        false,
        expansion,
    );
    do_floodfill(
        &mut layerstack,
        86,
        150,
        Color {
            r: 0.0,
            g: 1.0,
            b: 0.0,
            a: 1.0,
        },
        1,
        false,
        expansion,
    );
    do_floodfill(
        &mut layerstack,
        86,
        286,
        Color {
            r: 0.0,
            g: 0.0,
            b: 1.0,
            a: 1.0,
        },
        1,
        false,
        expansion,
    );
    do_floodfill(
        &mut layerstack,
        86,
        544,
        Color {
            r: 1.0,
            g: 1.0,
            b: 0.0,
            a: 1.0,
        },
        1,
        false,
        expansion,
    );

    do_floodfill(
        &mut layerstack,
        480,
        86,
        Color {
            r: 1.0,
            g: 0.0,
            b: 0.0,
            a: 1.0,
        },
        1,
        false,
        expansion,
    );
    do_floodfill(
        &mut layerstack,
        480,
        150,
        Color {
            r: 0.0,
            g: 1.0,
            b: 0.0,
            a: 1.0,
        },
        1,
        false,
        expansion,
    );
    do_floodfill(
        &mut layerstack,
        480,
        286,
        Color {
            r: 0.0,
            g: 0.0,
            b: 1.0,
            a: 1.0,
        },
        1,
        false,
        expansion,
    );
    do_floodfill(
        &mut layerstack,
        480,
        544,
        Color {
            r: 1.0,
            g: 1.0,
            b: 0.0,
            a: 1.0,
        },
        1,
        false,
        expansion,
    );

    // Test merged floodfill
    do_floodfill(
        &mut layerstack,
        854,
        86,
        Color {
            r: 1.0,
            g: 0.0,
            b: 0.0,
            a: 1.0,
        },
        2,
        true,
        expansion,
    );
    do_floodfill(
        &mut layerstack,
        854,
        150,
        Color {
            r: 0.0,
            g: 1.0,
            b: 0.0,
            a: 1.0,
        },
        2,
        true,
        expansion,
    );
    do_floodfill(
        &mut layerstack,
        854,
        286,
        Color {
            r: 0.0,
            g: 0.0,
            b: 1.0,
            a: 1.0,
        },
        2,
        true,
        expansion,
    );
    do_floodfill(
        &mut layerstack,
        854,
        544,
        Color {
            r: 1.0,
            g: 1.0,
            b: 0.0,
            a: 1.0,
        },
        2,
        true,
        expansion,
    );

    do_floodfill(
        &mut layerstack,
        1248,
        86,
        Color {
            r: 1.0,
            g: 0.0,
            b: 0.0,
            a: 1.0,
        },
        2,
        true,
        expansion,
    );
    do_floodfill(
        &mut layerstack,
        1248,
        150,
        Color {
            r: 0.0,
            g: 1.0,
            b: 0.0,
            a: 1.0,
        },
        2,
        true,
        expansion,
    );
    do_floodfill(
        &mut layerstack,
        1248,
        286,
        Color {
            r: 0.0,
            g: 0.0,
            b: 1.0,
            a: 1.0,
        },
        2,
        true,
        expansion,
    );
    do_floodfill(
        &mut layerstack,
        1248,
        544,
        Color {
            r: 1.0,
            g: 1.0,
            b: 0.0,
            a: 1.0,
        },
        2,
        true,
        expansion,
    );

    utils::save_layerstack(&layerstack, "example_floodfill.png");
}

fn do_floodfill(
    image: &mut LayerStack,
    x: i32,
    y: i32,
    color: Color,
    layer: LayerID,
    sample_merged: bool,
    expansion: i32,
) {
    let result = floodfill(image, x, y, color, 0.2, layer, sample_merged, 1000000);
    assert!(!result.oversize);

    let result = expand_floodfill(result, expansion);

    editlayer::draw_image8(
        image.root_mut().get_bitmaplayer_mut(1).unwrap(),
        0,
        &result.image.pixels,
        &Rectangle::new(
            result.x,
            result.y,
            result.image.width as i32,
            result.image.height as i32,
        ),
        0.5,
        Blendmode::Normal,
    );
}
