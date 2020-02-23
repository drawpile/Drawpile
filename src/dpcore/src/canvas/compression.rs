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

use crate::paint::tile::{Tile, TILE_LENGTH};
use crate::paint::{Color, Pixel, UserID};

use std::convert::TryInto;
use std::mem;
use tracing::warn;

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
