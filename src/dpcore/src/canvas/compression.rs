// This file is part of Drawpile.
// Copyright (C) 2020-2021 Calle Laakkonen
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

use crate::paint::tile::{Tile, TileData, TILE_LENGTH};
use crate::paint::{Color, Pixel, UserID};

use std::convert::TryInto;
use std::io::Write;
use std::{mem, slice};
use tracing::warn;

use deflate::{deflate_bytes_zlib, write::ZlibEncoder, Compression};
use inflate::inflate_bytes_zlib;

/// Decompress a Tile.
/// The input data should be prefixed with a 4 byte big endian
/// number indicating the expected length of the uncompressed data.
/// It should be 16384.
///
/// If the input vector is exactly 4 bytes long, it's interpreted
/// as an ARGB value and a tile filled with that color is returned.
pub fn decompress_tile(data: &[u8], user_id: UserID) -> Option<Tile> {
    if data.len() < 4 {
        warn!("decompress_tile: data too short!");
        return None;
    }

    let prefix = u32::from_be_bytes(data[..4].try_into().unwrap());
    if data.len() == 4 {
        return Some(Tile::new(&Color::from_argb32(prefix), user_id));
    }

    if prefix as usize != TILE_LENGTH * mem::size_of::<Pixel>() {
        warn!(
            "decompress_tile: wrong expected output length (was {})",
            prefix
        );
        return None;
    }

    let decompressed = match inflate_bytes_zlib(&data[4..]) {
        Ok(d) => d,
        Err(status) => {
            warn!("decompress_tile: decompression failed: {:?}", status);
            return None;
        }
    };

    if decompressed.len() != prefix as usize {
        warn!(
            "decompress_tile: decompressed length is not what was expected (was {})",
            decompressed.len()
        );
        return None;
    }

    let pixels =
        unsafe { std::slice::from_raw_parts(decompressed.as_ptr() as *const Pixel, TILE_LENGTH) };

    Some(Tile::from_data(pixels, user_id))
}

/// Compress a tile's content.
///
pub fn compress_tiledata(tiledata: &TileData) -> Vec<u8> {
    let pixelbytes = unsafe {
        slice::from_raw_parts(
            tiledata.pixels.as_ptr() as *const u8,
            TILE_LENGTH * mem::size_of::<Pixel>(),
        )
    };

    let compressed = deflate_bytes_zlib(pixelbytes);

    // For compatibility with Qt's compresssion function, add a prefix
    // containing the expected length of the decompressed buffer.
    // We can probably drop this in the next protocol change.
    let prefix = u32::to_be_bytes(4 * TILE_LENGTH as u32);
    let mut prefixed_data = Vec::<u8>::with_capacity(prefix.len() + compressed.len());
    prefixed_data.extend_from_slice(&prefix);
    prefixed_data.extend_from_slice(&compressed);

    prefixed_data
}

/// Compress a tile's content
///
/// If the tile is filled with solid color, the pixel data is not compressed
/// but the color value is returned.
pub fn compress_tile(tile: &Tile) -> Vec<u8> {
    if let Some(c) = tile.solid_color() {
        c.as_argb32().to_be_bytes().to_vec()
    } else {
        compress_tiledata(tile.data())
    }
}

pub fn decompress_image(data: &[u8], expected_len: usize) -> Option<Vec<Pixel>> {
    if data.len() < 4 {
        warn!("decompress_image: data too short!");
        return None;
    }

    let expected_bytes_len = expected_len * mem::size_of::<Pixel>();
    let prefix = u32::from_be_bytes(data[..4].try_into().unwrap());
    if prefix as usize != expected_bytes_len {
        warn!(
            "decompress_image: wrong length (was {}, expected {})",
            prefix, expected_bytes_len
        );
        return None;
    }

    let decompressed = match inflate_bytes_zlib(&data[4..]) {
        Ok(d) => d,
        Err(status) => {
            warn!("decompress_image: decompression failed: {:?}", status);
            return None;
        }
    };

    if decompressed.len() != expected_bytes_len {
        warn!(
            "decompress_image: decompressed length is not what was expected (was {}, expected {})",
            decompressed.len(),
            expected_bytes_len
        );
        return None;
    }

    let pixels: Vec<Pixel> = decompressed
        .chunks_exact(4)
        .map(|p| p.try_into().unwrap())
        .collect();
    return Some(pixels);
}

pub fn compress_image(pixels: &[Pixel]) -> Vec<u8> {
    let pixelbytes = unsafe {
        slice::from_raw_parts(
            pixels.as_ptr() as *const u8,
            pixels.len() * mem::size_of::<Pixel>(),
        )
    };

    let compressed = deflate_bytes_zlib(pixelbytes);

    // For compatibility with Qt's compresssion function, add a prefix
    // containing the expected length of the decompressed buffer.
    // We can probably drop this in the next protocol change.
    let prefix = u32::to_be_bytes(pixelbytes.len() as u32);
    let mut prefixed_data = Vec::<u8>::with_capacity(prefix.len() + compressed.len());
    prefixed_data.extend_from_slice(&prefix);
    prefixed_data.extend_from_slice(&compressed);

    prefixed_data
}

pub struct ImageCompressor {
    encoder: ZlibEncoder<Vec<u8>>,
    uncompressed_len: usize,
}

impl ImageCompressor {
    fn reserve_header() -> Vec<u8> {
        vec![0, 0, 0, 0]
    }

    pub fn new() -> Self {
        Self {
            encoder: ZlibEncoder::new(Self::reserve_header(), Compression::Default),
            uncompressed_len: 0,
        }
    }

    pub fn add(&mut self, data: &[Pixel]) {
        let data = unsafe {
            slice::from_raw_parts(
                data.as_ptr() as *const u8,
                data.len() * mem::size_of::<Pixel>(),
            )
        };

        // encoding shouldn't fail in normal conditions
        self.encoder.write_all(data).expect("Zlib Encoder failed!");
        self.uncompressed_len += data.len();
    }

    pub fn finish(self) -> Vec<u8> {
        let mut vec = self.encoder.finish().expect("Zlib Encoder failed!");

        // For compatibility with Qt's compresssion function, add a prefix
        // containing the expected length of the decompressed buffer.
        // We can probably drop this in the next protocol change.
        vec[..4].copy_from_slice(&u32::to_be_bytes(self.uncompressed_len as u32));

        vec
    }
}
