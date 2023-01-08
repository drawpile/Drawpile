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

use dpcore::paint::color::*;
use dpcore::paint::tile::{Tile, TileData, TILE_SIZE};
use dpcore::paint::{AoE, BitmapLayer, FlattenedTileIterator, LayerStack, LayerViewOptions};
use image::{ImageBuffer, RgbaImage};

fn copy_tile_to(dest: &mut [u8], stride: u32, tile: &TileData, tx: u32, ty: u32) {
    let mut dest_offset = ty * TILE_SIZE * stride * 4 + tx * TILE_SIZE * 4;

    for y in 0..TILE_SIZE {
        for x in 0..TILE_SIZE {
            let px = tile.pixels[(y * TILE_SIZE + x) as usize];
            dest[(dest_offset + x * 4) as usize] = channel15_to_8(px[RED_CHANNEL]);
            dest[(dest_offset + x * 4 + 1) as usize] = channel15_to_8(px[GREEN_CHANNEL]);
            dest[(dest_offset + x * 4 + 2) as usize] = channel15_to_8(px[BLUE_CHANNEL]);
            dest[(dest_offset + x * 4 + 3) as usize] = channel15_to_8(px[ALPHA_CHANNEL]);
        }
        dest_offset += stride * 4;
    }
}

fn u8_mult(a: u8, b: u8) -> u8 {
    let c = a as u32 * b as u32 + 0x80u32;
    (((c >> 8) + c) >> 8) as u8
}

#[allow(dead_code)]
pub fn load_image(filename: &str) -> (Vec<Pixel8>, i32, i32) {
    let img = image::open(filename).expect("couldn't load image");
    let rgba = img.as_rgba8().unwrap();

    let mut argb_data = Vec::<Pixel8>::with_capacity((rgba.width() * rgba.height()) as usize);

    for p in rgba.pixels() {
        argb_data.push([
            u8_mult(p.0[3], p.0[2]),
            u8_mult(p.0[3], p.0[1]),
            u8_mult(p.0[3], p.0[0]),
            p.0[3],
        ]);
    }

    (argb_data, rgba.width() as i32, rgba.height() as i32)
}

#[allow(dead_code)]
pub fn save_layer(layer: &BitmapLayer, filename: &str) {
    // This is just to make copy_tile_to simpler:
    assert!(layer.width() % TILE_SIZE == 0);
    assert!(layer.height() % TILE_SIZE == 0);

    let mut rgba = vec![0u8; (layer.width() * layer.height() * 4) as usize];

    for ty in 0..(layer.height() / TILE_SIZE) {
        for tx in 0..(layer.width() / TILE_SIZE) {
            if let Tile::Bitmap(td) = layer.tile(tx, ty) {
                copy_tile_to(&mut rgba, layer.width(), td, tx, ty);
            }
        }
    }

    let img: RgbaImage = ImageBuffer::from_vec(layer.width(), layer.height(), rgba).unwrap();

    println!("Writing {}", filename);
    img.save(filename).unwrap();
}

#[allow(dead_code)]
pub fn save_layerstack(layerstack: &LayerStack, filename: &str) {
    // This is just to make copy_tile_to simpler:
    let w = layerstack.root().width();
    let h = layerstack.root().height();
    assert!(w % TILE_SIZE == 0);
    assert!(h % TILE_SIZE == 0);

    let mut rgba = vec![0u8; (w * h * 4) as usize];

    for (i, j, tile) in
        FlattenedTileIterator::new(layerstack, &LayerViewOptions::default(), AoE::Everything)
    {
        copy_tile_to(
            &mut rgba,
            layerstack.root().width(),
            &tile,
            i as u32,
            j as u32,
        );
    }

    let img: RgbaImage = ImageBuffer::from_vec(w, h, rgba).unwrap();

    println!("Writing {}", filename);
    img.save(filename).unwrap();
}
