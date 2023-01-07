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

use dpcore::paint::color::*;
use dpcore::paint::Image8;
use image::RgbaImage;

pub fn to_dpimage(img: &RgbaImage) -> Image8 {
    let mut dpimg = Image8::new(img.width() as usize, img.height() as usize);

    let pixels = bytemuck::cast_slice::<_, Pixel8>(img.as_raw());

    // Premultiply pixel values
    dpimg
        .pixels
        .iter_mut()
        .zip(pixels.iter())
        .for_each(|(d, s)| {
            let a = s[3] as u32;
            d[RED_CHANNEL] = u8_mult(s[0] as u32, a);
            d[GREEN_CHANNEL] = u8_mult(s[1] as u32, a);
            d[BLUE_CHANNEL] = u8_mult(s[2] as u32, a);
            d[ALPHA_CHANNEL] = s[3];
        });

    dpimg
}

pub fn from_dpimage(img: &Image8) -> RgbaImage {
    let mut rgba = Vec::with_capacity(img.width * img.height * 4);

    // Unpremultiply pixel values
    for px in img.pixels.iter() {
        let a = px[ALPHA_CHANNEL] as u32;
        let a = if a > 0 { (255 * 255 + a / 2) / a } else { 0 };

        rgba.push(u8_mult(px[RED_CHANNEL] as u32, a));
        rgba.push(u8_mult(px[GREEN_CHANNEL] as u32, a));
        rgba.push(u8_mult(px[BLUE_CHANNEL] as u32, a));
        rgba.push(px[ALPHA_CHANNEL]);
    }

    image::RgbaImage::from_raw(img.width as u32, img.height as u32, rgba).unwrap()
}

fn u8_mult(a: u32, b: u32) -> u8 {
    let c = a * b + 0x80;
    (((c >> 8) + c) >> 8) as u8
}
