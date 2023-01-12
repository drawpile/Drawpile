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

use dpcore::paint::{editlayer, BitmapLayer, Blendmode, Color, Rectangle, Tile};

mod utils;

fn main() {
    let mut layer = BitmapLayer::new(0, 256, 256, Tile::new(&Color::rgb8(255, 255, 255), 0));

    let (image, w, h) = utils::load_image_data(include_bytes!("testdata/logo.png"));

    editlayer::draw_image8(
        &mut layer,
        0,
        &image,
        &Rectangle::new(256 - w * 2 / 3, 256 - h * 2 / 3, w, h),
        1.0,
        Blendmode::Replace,
    );

    editlayer::draw_image8(
        &mut layer,
        0,
        &image,
        &Rectangle::new(256 / 2 - w / 2, 256 / 2 - h / 2, w, h),
        0.5,
        Blendmode::Normal,
    );

    editlayer::draw_image8(
        &mut layer,
        0,
        &image,
        &Rectangle::new(-w / 2, -h / 2, w, h),
        1.0,
        Blendmode::Normal,
    );

    utils::save_layer(&layer, "example_layer_drawimage.png");
}
