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

use std::convert::TryFrom;
use std::sync::Arc;

use super::aoe::{AoE, TileMap};
use super::blendmode::Blendmode;
use super::brushmask::BrushMask;
use super::color::{Color, Pixel, ZERO_PIXEL};
use super::rect::{Rectangle, Size};
use super::rectiter::{MutableRectIterator, RectIterator};
use super::tile::{Tile, TileData, TILE_SIZE, TILE_SIZEI};
use super::tileiter::{MutableTileIterator, TileIterator};
use super::{Image, InternalLayerID};

/// A tiled image layer.
///
/// When modifying the layer, the functions in editlayer module should be used.
/// These functions return an Area Of Effect value that can be used to notify
/// layer observers of the changes.
///
#[derive(Clone)]
pub struct Layer {
    pub id: InternalLayerID,
    pub title: String,
    pub opacity: f32,
    pub hidden: bool,
    pub censored: bool,
    pub fixed: bool,
    pub blendmode: Blendmode,
    width: u32,
    height: u32,
    tiles: Arc<Vec<Tile>>,
    sublayers: Vec<Arc<Layer>>,
}

impl Layer {
    /// Construct a new layer filled with the given color
    pub fn new(id: InternalLayerID, width: u32, height: u32, fill: Tile) -> Layer {
        Layer {
            id,
            title: String::new(),
            opacity: 1.0,
            hidden: false,
            censored: false,
            fixed: false,
            blendmode: Blendmode::Normal,
            width,
            height,
            tiles: Arc::new(vec![
                fill;
                (Tile::div_up(width) * Tile::div_up(height)) as usize
            ]),
            sublayers: vec![],
        }
    }

    /// Build a layer from raw pixel data
    /// This is typically used for scratch layers as a part of some
    /// larger image manipulation process.
    pub fn from_image(pixels: &[Pixel], width: u32, height: u32) -> Layer {
        let xtiles = Tile::div_up(width);
        let ytiles = Tile::div_up(height);

        let mut layer = Layer::new(InternalLayerID(0), width, height, Tile::Blank);

        let imagerect = Rectangle::new(0, 0, width as i32, height as i32);

        let tilevec = Arc::make_mut(&mut layer.tiles);
        for ty in 0..ytiles {
            for tx in 0..xtiles {
                let srcrect = Rectangle::new(
                    (tx * TILE_SIZE) as i32,
                    (ty * TILE_SIZE) as i32,
                    TILE_SIZE as i32,
                    TILE_SIZE as i32,
                )
                .intersected(&imagerect)
                .unwrap();
                let destrect =
                    srcrect.offset(-((tx * TILE_SIZE) as i32), -((ty * TILE_SIZE) as i32));

                let tile = tilevec[(ty * xtiles + tx) as usize].rect_iter_mut(0, &destrect, true);
                let src = RectIterator::from_rectangle(pixels, width as usize, &srcrect);

                for (destrow, srcrow) in tile.zip(src) {
                    destrow.copy_from_slice(srcrow);
                }
            }
        }

        layer.optimize(&AoE::Everything);
        layer
    }

    /// Get a cropped image
    ///
    /// For efficiency, the image is cropped at tile boundaries.
    /// Returns: (image, x coordinate, y coordinate)
    pub fn to_cropped_image(&self) -> (Image, i32, i32) {
        // Find bounding rectangle of non-blank tiles
        let xtiles = Tile::div_up(self.width) as usize;
        let ytiles = Tile::div_up(self.height) as usize;
        let mut top = ytiles;
        let mut btm = 0;
        let mut left = xtiles;
        let mut right = 0;

        for y in 0..ytiles {
            for x in 0..xtiles {
                if !self.tiles[y * xtiles + x].is_blank() {
                    left = left.min(x);
                    right = right.max(x);
                    top = top.min(y);
                    btm = btm.max(y);
                }
            }
        }

        if top == ytiles {
            return (Image::default(), 0, 0);
        }

        // Copy tiles to image
        let tw = TILE_SIZE as usize;
        let width = (right - left + 1) * tw;
        let height = (btm - top + 1) * tw;
        let mut image = vec![ZERO_PIXEL; width * height];
        for y in top..=btm {
            for x in left..=right {
                match &self.tiles[y * xtiles + x] {
                    Tile::Bitmap(td) => {
                        for line in 0..tw {
                            let dest_offset = ((y - top) * tw + line) * width + (x - left) * tw;
                            let src_offset = line * tw;

                            image[dest_offset..dest_offset + tw]
                                .copy_from_slice(&td.pixels[src_offset..src_offset + tw]);
                        }
                    }
                    Tile::Blank => {}
                }
            }
        }

        (
            Image {
                pixels: image,
                width,
                height,
            },
            (left * tw) as i32,
            (top * tw) as i32,
        )
    }

    /// Copy pixels from the given rectangle to the pixel slice
    ///
    /// Note: the destination should be initialized to zero, as this function
    /// skips blank tiles.
    /// The rectangle must be fully within the layer bounds.
    /// The length of the pixel slice must be rect width*height.
    pub fn to_pixels(&self, rect: Rectangle, pixels: &mut [Pixel]) -> Result<(), &str> {
        if !rect.in_bounds(self.size()) {
            return Err("source rectangle out of bounds");
        }
        if pixels.len() != (rect.w * rect.h) as usize {
            return Err("pixel slice length does not match source rectangle size");
        }

        for (i, j, tile) in self.tile_rect(&rect) {
            match tile {
                Tile::Blank => {
                    continue;
                }
                _ => (),
            };

            // This tile's bounding rectangle
            let tilerect = Rectangle::tile(i, j, TILE_SIZEI);

            // The rectangle of interest in the layer
            let layerrect = rect.intersected(&tilerect).unwrap();

            // The source rectangle in the tile
            let srcrect = layerrect.offset(-tilerect.x, -tilerect.y);

            // The destination rectangle in the pixel buffer
            let pixelrect = layerrect.offset(-rect.x, -rect.y);

            // Copy pixels from tile to image
            let srciter = tile.rect_iter(&srcrect);
            let destiter = MutableRectIterator::new(
                pixels,
                rect.w as usize,
                pixelrect.x as usize,
                pixelrect.y as usize,
                pixelrect.w as usize,
                pixelrect.h as usize,
            );

            destiter
                .zip(srciter)
                .for_each(|(d, s)| d.copy_from_slice(s));
        }

        Ok(())
    }

    pub fn to_image(&self, rect: Rectangle) -> Result<Image, &str> {
        let rect = match rect.cropped(self.size()) {
            Some(r) => r,
            None => {
                return Err("source rectangle out of bounds");
            }
        };

        let mut img = Image::new(rect.w as usize, rect.h as usize);

        self.to_pixels(rect, &mut img.pixels)?;
        Ok(img)
    }

    /// Get mutable access to a sublayer with the given ID
    ///
    /// If the sublayer does not exist already, it will be created and added to the list of sublayers.
    ///
    /// By convention, ID 0 is not used. Sublayers with positive IDs are used for indirect
    /// drawing (matching user IDs) and sublayers with negative IDs are for local previews.
    pub fn get_or_create_sublayer(&mut self, id: InternalLayerID) -> &mut Layer {
        assert!(id != 0, "Sublayer ID 0 is not allowed");

        if let Some(i) = self.sublayers.iter().position(|sl| sl.id == id) {
            return Arc::make_mut(&mut self.sublayers[i]);
        }
        self.sublayers.push(Arc::new(Layer::new(
            id,
            self.width,
            self.height,
            Tile::Blank,
        )));
        let last = self.sublayers.len() - 1;
        Arc::make_mut(&mut self.sublayers[last])
    }

    /// Find and remove a sublayer with the given ID (if it exists)
    ///
    /// Note: you should not typically need to call this directly.
    /// Instead, use `merge_sublayer` or `remove_sublayer` from `editlayer` module
    pub fn take_sublayer(&mut self, id: InternalLayerID) -> Option<Arc<Layer>> {
        if let Some(i) = self.sublayers.iter().position(|sl| sl.id == id) {
            Some(self.sublayers.remove(i))
        } else {
            None
        }
    }

    /// Return a list of visible sublayers this layer has
    pub fn sublayer_ids(&self) -> Vec<InternalLayerID> {
        self.sublayers.iter().map(|sl| sl.id).collect()
    }

    /// Check if a sublayer with the given ID exists
    pub fn has_sublayer(&self, id: InternalLayerID) -> bool {
        self.sublayers.iter().any(|sl| sl.id == id)
    }

    pub fn width(&self) -> u32 {
        self.width
    }

    pub fn height(&self) -> u32 {
        self.height
    }

    pub fn size(&self) -> Size {
        Size::new(self.width as i32, self.height as i32)
    }

    /// A layer is visible when it's not explicitly hidden and it's opacity is greater than zero
    ///
    /// The `hidden` flag is typically used as a local-only flag that allows a layer to be
    /// hidden just for the current user.
    pub fn is_visible(&self) -> bool {
        !self.hidden && self.opacity > 0.0
    }

    /// Return the pixel at the given coordinates
    pub fn pixel_at(&self, x: u32, y: u32) -> Pixel {
        let ti = x / TILE_SIZE;
        let tj = y / TILE_SIZE;
        let tx = x - ti * TILE_SIZE;
        let ty = y - tj * TILE_SIZE;

        self.tile(ti, tj).pixel_at(tx, ty)
    }

    /// Get a weighted average of the color using a default sampling mask
    pub fn sample_color(&self, x: i32, y: i32, dia: i32) -> Color {
        if dia <= 1 {
            if x < 0 || y < 0 || x as u32 > self.width || y as u32 > self.height {
                Color::TRANSPARENT
            } else {
                Color::from_pixel(self.pixel_at(x as u32, y as u32))
            }
        } else {
            let mask = BrushMask::new_round_pixel(dia as u32, 1.0);
            self.sample_dab_color(x, y, &mask)
        }
    }

    /// Get a weighted average of the color under the dab mask
    pub fn sample_dab_color(&self, x: i32, y: i32, dab: &BrushMask) -> Color {
        let sample_rect = match Rectangle::new(x, y, dab.diameter as i32, dab.diameter as i32)
            .cropped(self.size())
        {
            Some(r) => r,
            None => return Color::TRANSPARENT,
        };

        let tx0 = sample_rect.x / TILE_SIZEI;
        let ty0 = sample_rect.y / TILE_SIZEI;
        let tx1 = sample_rect.right() / TILE_SIZEI;
        let ty1 = sample_rect.bottom() / TILE_SIZEI;

        let mut sum_weight = 0.0;
        let mut sum_color = Color::TRANSPARENT;

        for tx in tx0..=tx1 {
            for ty in ty0..=ty1 {
                let tile = self.tile(tx as u32, ty as u32);
                let tile_rect = Rectangle::tile(tx, ty, TILE_SIZEI);
                let subrect = sample_rect.intersected(&tile_rect).unwrap();
                let rect_in_tile = subrect.offset(-tile_rect.x, -tile_rect.y);
                let rect_in_dab = subrect.offset(-sample_rect.x, -sample_rect.y);
                for (pix, mask) in tile
                    .rect_iter(&rect_in_tile)
                    .flatten()
                    .zip(dab.rect_iter(&rect_in_dab).flatten())
                {
                    let m = *mask as f32 / 255.0;
                    let c = Color::from_premultiplied_pixel(*pix);
                    sum_weight += m;
                    sum_color.r += c.r * m;
                    sum_color.g += c.g * m;
                    sum_color.b += c.b * m;
                    sum_color.a += c.a * m;
                }
            }
        }

        // There must be at least some alpha for the results to make sense
        if sum_color.a < ((dab.diameter * dab.diameter) as f32 * 0.1) {
            return Color::TRANSPARENT;
        }

        sum_color.r /= sum_weight;
        sum_color.g /= sum_weight;
        sum_color.b /= sum_weight;
        sum_color.a /= sum_weight;

        // Unpremultiply
        sum_color.r = 1.0f32.min(sum_color.r / sum_color.a);
        sum_color.g = 1.0f32.min(sum_color.g / sum_color.a);
        sum_color.b = 1.0f32.min(sum_color.b / sum_color.a);

        sum_color
    }

    /// Check if every tile of this layer is the same
    pub fn same_tile(&self) -> Option<Tile> {
        if self.tiles.is_empty() {
            return None;
        }
        let first = &self.tiles[0];
        if self.tiles[1..].iter().any(|t| t != first) {
            return None;
        }

        Some(first.clone())
    }

    /// Check if this entire layer is filled with a solid color
    pub fn solid_color(&self) -> Option<Color> {
        if self.tiles.is_empty() {
            return Some(Color::TRANSPARENT);
        }
        let c = self.tiles[0].solid_color();
        if c.is_none() || self.tiles[1..].iter().any(|t| t.solid_color() != c) {
            None
        } else {
            c
        }
    }

    /// Return a bitmap of non-blank tiles
    pub fn nonblank_tilemap(&self) -> TileMap {
        TileMap {
            tiles: self.tiles.iter().map(|t| *t != Tile::Blank).collect(),
            w: Tile::div_up(self.width),
            h: Tile::div_up(self.height),
        }
    }

    /// Return the tile at the given index
    pub fn tile(&self, i: u32, j: u32) -> &Tile {
        debug_assert!(i * TILE_SIZE < self.width);
        debug_assert!(j * TILE_SIZE < self.height);
        let xtiles = Tile::div_up(self.width);
        &self.tiles[(j * xtiles + i) as usize]
    }

    /// Return a mutable reference to a tile at the given index
    pub fn tile_mut(&mut self, i: u32, j: u32) -> &mut Tile {
        debug_assert!(i * TILE_SIZE < self.width);
        debug_assert!(j * TILE_SIZE < self.height);
        let xtiles = Tile::div_up(self.width);
        let v = Arc::make_mut(&mut self.tiles);
        &mut v[(j * xtiles + i) as usize]
    }

    /// Replace a tile at the given index
    pub fn replace_tile(&mut self, i: u32, j: u32, tile: Tile) {
        debug_assert!(i * TILE_SIZE < self.width);
        debug_assert!(j * TILE_SIZE < self.height);
        let xtiles = Tile::div_up(self.width);
        let v = Arc::make_mut(&mut self.tiles);
        v[(j * xtiles + i) as usize] = tile;
    }

    /// Get direct access to the tile vector.
    /// You normally shouldn't use this directly.
    pub fn tilevec(&self) -> &Vec<Tile> {
        &self.tiles
    }

    /// Get direct mutable access to the tile vector.
    /// You normally shouldn't use this directly. Instead, use the
    /// functions in `editlayer` module.
    pub fn tilevec_mut(&mut self) -> &mut Vec<Tile> {
        Arc::make_mut(&mut self.tiles)
    }

    /// Return an iterator to the tiles that intersect the given
    /// rectangle (in normal pixel coordinates)
    pub fn tile_rect(&self, r: &Rectangle) -> TileIterator<Tile> {
        assert!(r.in_bounds(self.size()));

        let tx0 = (r.x / TILE_SIZEI) as usize;
        let tx1 = (r.right() / TILE_SIZEI) as usize;
        let ty0 = (r.y / TILE_SIZEI) as usize;
        let ty1 = (r.bottom() / TILE_SIZEI) as usize;
        let stride = Tile::div_up(self.width) as usize;

        TileIterator::new(
            self.tilevec(),
            stride,
            tx0,
            ty0,
            tx1 - tx0 + 1,
            ty1 - ty0 + 1,
        )
    }

    /// Return a mutable iterator to the tiles that intersect the given
    /// rectangle (in normal pixel coordinates)
    pub fn tile_rect_mut(&mut self, r: &Rectangle) -> MutableTileIterator<Tile> {
        assert!(r.in_bounds(self.size()));

        let tx0 = (r.x / TILE_SIZEI) as usize;
        let tx1 = (r.right() / TILE_SIZEI) as usize;
        let ty0 = (r.y / TILE_SIZEI) as usize;
        let ty1 = (r.bottom() / TILE_SIZEI) as usize;
        let stride = Tile::div_up(self.width) as usize;

        MutableTileIterator::new(
            self.tilevec_mut(),
            stride,
            tx0,
            ty0,
            tx1 - tx0 + 1,
            ty1 - ty0 + 1,
        )
    }

    pub fn flatten_tile(&self, destination: &mut TileData, i: u32, j: u32) {
        if !self.is_visible() {
            return;
        }

        // TODO censor
        if self.sublayers.is_empty() {
            // No sublayers: just composite this one as is
            destination.merge_tile(self.tile(i, j), self.opacity, self.blendmode);
        } else {
            // Sublayers present: compositing needed
            let mut tmp = self.tile(i, j).clone_data();
            for sublayer in self.sublayers.iter() {
                if sublayer.is_visible() {
                    tmp.merge_tile(sublayer.tile(i, j), sublayer.opacity, sublayer.blendmode);
                }
            }

            // TODO tint, highlight and onionskin

            destination.merge_data(&tmp, self.opacity, self.blendmode);
        }
    }

    /// Call optimize on every tile in the given area.
    /// This will release memory and speed up rendering, as blank
    /// tiles can be skipped.
    pub fn optimize(&mut self, area: &AoE) {
        use AoE::*;
        match area {
            Nothing => (),
            Bounds(r) => self.tile_rect_mut(r).for_each(|(_, _, t)| t.optimize()),
            // Note: when using a bitmap, we need to iterate through the
            // entire vector anyway and an optimize call is cheap enough
            // that there's no point to the extra check.
            _ => {
                Arc::make_mut(&mut self.tiles)
                    .iter_mut()
                    .for_each(|t| t.optimize());
            }
        }
    }

    /// Do a shallow comparison between these layers and return the difference
    pub fn compare(&self, other: &Layer) -> AoE {
        if Arc::ptr_eq(&self.tiles, &other.tiles) {
            return AoE::Nothing;
        }
        if self.width != other.width || self.height != other.height {
            return AoE::Resize(0, 0, self.size());
        }

        TileMap {
            tiles: self
                .tiles
                .iter()
                .zip(other.tiles.iter())
                .map(|(a, b)| !a.ptr_eq(b))
                .collect(),
            w: Tile::div_up(self.width),
            h: Tile::div_up(self.height),
        }
        .into()
    }

    /// Return a new layer with the size adjusted by the given values
    ///
    /// The new layer will contain the same content as this one, but offset and possibly cropped.
    /// If the layer is filled with a solid color, the resized layer will also be fully filled
    /// with that color. Otherwise, the expanded areas will be filled with transparency.
    pub fn resized(&self, top: i32, right: i32, bottom: i32, left: i32) -> Layer {
        let new_width = u32::try_from(left + self.width as i32 + right).unwrap();
        let new_height = u32::try_from(top + self.height as i32 + bottom).unwrap();

        let new_tiles = if let Some(c) = self.solid_color() {
            // The fastest case: this layer is filled with solid color
            Arc::new(vec![
                Tile::new(&c, 0);
                (Tile::div_up(new_width) * Tile::div_up(new_height))
                    as usize
            ])
        } else if (left % TILE_SIZEI) == 0 && (top % TILE_SIZEI) == 0 {
            // Tile aligned resize. Existing tiles can be reused.
            self.resized_fast(left, top, new_width, new_height)
        } else {
            // Uh oh, top/left change not evenly divisible by tile size
            // means we have to rebuild all the tiles
            self.resized_slow(left, top, new_width, new_height)
        };

        Layer {
            title: self.title.clone(),
            width: new_width,
            height: new_height,
            tiles: new_tiles,
            sublayers: self
                .sublayers
                .iter()
                .map(|sl| Arc::new(sl.resized(top, right, bottom, left)))
                .collect(),
            ..*self
        }
    }

    fn resized_slow(&self, offx: i32, offy: i32, w: u32, h: u32) -> Arc<Vec<Tile>> {
        let oldxtiles = Tile::div_up(self.width) as i32;
        let newxtiles = Tile::div_up(w) as i32;
        let newytiles = Tile::div_up(h) as i32;
        let mut new_vec = Arc::new(vec![Tile::Blank; (newxtiles * newytiles) as usize]);
        let tiles = Arc::make_mut(&mut new_vec);

        // Iterate through the original image. Most of the time, the canvas
        // is being expanded so this is the set of tiles we'd iterate over anyway.
        for (index, tile) in self.tiles.iter().enumerate() {
            if *tile == Tile::Blank {
                continue;
            }

            let source_tile_geometry = Rectangle::tile(
                index as i32 % oldxtiles,
                index as i32 / oldxtiles,
                TILE_SIZEI,
            );

            // Where the pixel data should be moved.
            // It might be out of bounds if the layer is being contracted.
            let target_rect = source_tile_geometry.offset(offx, offy);

            if let Some(cropped) = target_rect.cropped(Size::new(w as i32, h as i32)) {
                // It appears to be in bounds.

                // Depending on the offset, the source tile will overlap
                // with either 2 or 4 destination tiles
                let dx0 = (cropped.x / TILE_SIZEI) as usize;
                let dx1 = (cropped.right() / TILE_SIZEI) as usize;
                let dy0 = (cropped.y / TILE_SIZEI) as usize;
                let dy1 = (cropped.bottom() / TILE_SIZEI) as usize;

                for (i, j, dest_tile) in MutableTileIterator::new(
                    tiles,
                    newxtiles as usize,
                    dx0,
                    dy0,
                    dx1 - dx0 + 1,
                    dy1 - dy0 + 1,
                ) {
                    let destination_tile_geometry = Rectangle::tile(i, j, TILE_SIZEI);

                    // The half or quarter destination tile
                    let subrect = destination_tile_geometry.intersected(&target_rect).unwrap();

                    // Destination rectangle inside the destination tile
                    let dest_tile_rect =
                        subrect.offset(-destination_tile_geometry.x, -destination_tile_geometry.y);

                    // Source rectangle inside the source tile
                    let source_tile_rect = subrect.offset(-target_rect.x, -target_rect.y);

                    dest_tile
                        .rect_iter_mut(tile.last_touched_by(), &dest_tile_rect, true)
                        .zip(tile.rect_iter(&source_tile_rect))
                        .for_each(|(d, s)| d.copy_from_slice(s));
                }
            }
        }

        new_vec
    }

    fn resized_fast(&self, offx: i32, offy: i32, w: u32, h: u32) -> Arc<Vec<Tile>> {
        debug_assert!(offx % TILE_SIZEI == 0);
        debug_assert!(offy % TILE_SIZEI == 0);
        let oldxtiles = Tile::div_up(self.width) as i32;
        let oldytiles = Tile::div_up(self.height) as i32;
        let newxtiles = Tile::div_up(w) as i32;
        let newytiles = Tile::div_up(h) as i32;
        let mut new_vec = Arc::new(vec![Tile::Blank; (newxtiles * newytiles) as usize]);

        let xt_off = offx / TILE_SIZEI;
        let yt_off = offy / TILE_SIZEI;

        let tiles = Arc::make_mut(&mut new_vec);

        for y in yt_off.max(0)..newytiles.min(oldytiles + yt_off) {
            let sy = y - yt_off;
            for x in xt_off.max(0)..newxtiles.min(oldxtiles + xt_off) {
                let sx = x - xt_off;
                tiles[(y * newxtiles + x) as usize] = self.tile(sx as u32, sy as u32).clone();
            }
        }

        new_vec
    }

    #[cfg(test)]
    fn refcount(&self) -> usize {
        Arc::strong_count(&self.tiles) + Arc::weak_count(&self.tiles)
    }
}

#[cfg(test)]
mod tests {
    use super::super::color::{WHITE_PIXEL, ZERO_PIXEL};
    use super::*;
    use bitvec::prelude::*;

    #[test]
    fn test_tilevector_cow() {
        let mut layer = Layer::new(
            InternalLayerID(0),
            100,
            64,
            Tile::new(
                &Color {
                    r: 1.0,
                    g: 0.0,
                    b: 0.0,
                    a: 1.0,
                },
                0,
            ),
        );
        let layer2 = layer.clone();
        assert_eq!(layer.refcount(), 2);

        // Tile vectors are shared at this point
        assert_eq!(layer.tiles[0].refcount(), 2);
        assert_eq!(layer.compare(&layer2), AoE::Nothing);

        // Changing a tile makes the vectors unique:
        // there are now more references to the same tile data
        *layer.tile_mut(0, 0) = Tile::Blank;
        assert_eq!(layer.refcount(), 1);
        assert_eq!(layer2.tiles[0].refcount(), 3);

        assert_eq!(
            layer.compare(&layer2),
            AoE::Bitmap(TileMap {
                tiles: bitvec![1, 0],
                w: 2,
                h: 1,
            })
        );
    }

    #[test]
    fn test_from_small_image() {
        #[rustfmt::skip]
        let image = [
            [0, 0, 0, 1], [0, 0, 0, 2], [0, 0, 0, 3],
            [0, 0, 1, 1], [0, 0, 1, 2], [0, 0, 1, 3],
        ];
        let layer = Layer::from_image(&image, 3, 2);
        assert_eq!(layer.pixel_at(0, 0), [0, 0, 0, 1]);
        assert_eq!(layer.pixel_at(1, 0), [0, 0, 0, 2]);
        assert_eq!(layer.pixel_at(2, 0), [0, 0, 0, 3]);
        assert_eq!(layer.pixel_at(3, 0), [0, 0, 0, 0]);
        assert_eq!(layer.pixel_at(0, 1), [0, 0, 1, 1]);
        assert_eq!(layer.pixel_at(1, 1), [0, 0, 1, 2]);
        assert_eq!(layer.pixel_at(2, 1), [0, 0, 1, 3]);
        assert_eq!(layer.pixel_at(3, 1), [0, 0, 0, 0]);
        assert_eq!(layer.pixel_at(0, 2), [0, 0, 0, 0]);
    }

    #[test]
    fn test_solid_expand() {
        let r = Color::rgb8(255, 0, 0);
        let layer = Layer::new(InternalLayerID(0), TILE_SIZE, TILE_SIZE, Tile::new(&r, 0));
        let layer2 = layer.resized(10, 10, 0, 0);
        for t in layer2.tiles.iter() {
            assert_eq!(t.solid_color(), Some(r));
        }
    }

    #[test]
    fn test_fast_expand() {
        let t = Color::TRANSPARENT;

        let mut layer = Layer::new(InternalLayerID(0), TILE_SIZE, TILE_SIZE, Tile::Blank);
        // Change a pixel so the whole layer won't be of uniform color
        layer
            .tile_mut(0, 0)
            .rect_iter_mut(0, &Rectangle::new(1, 1, 1, 1), false)
            .next()
            .unwrap()[0] = WHITE_PIXEL;

        let layer2 = layer.resized(TILE_SIZEI, TILE_SIZEI, 2 * TILE_SIZEI, TILE_SIZEI);

        // Should look like:
        // 000
        // 0X0
        // 000
        // 000

        assert_eq!(layer2.width(), TILE_SIZE * 3);
        assert_eq!(layer2.height(), TILE_SIZE * 4);
        assert_eq!(layer2.tile(0, 0).solid_color(), Some(t));
        assert_eq!(layer2.tile(1, 0).solid_color(), Some(t));
        assert_eq!(layer2.tile(2, 0).solid_color(), Some(t));

        assert_eq!(layer2.tile(0, 1).solid_color(), Some(t));
        assert_eq!(layer2.tile(1, 1).solid_color(), None);
        assert_eq!(layer2.tile(2, 1).solid_color(), Some(t));

        assert_eq!(layer2.tile(0, 2).solid_color(), Some(t));
        assert_eq!(layer2.tile(1, 2).solid_color(), Some(t));
        assert_eq!(layer2.tile(2, 2).solid_color(), Some(t));

        assert_eq!(layer2.tile(0, 3).solid_color(), Some(t));
        assert_eq!(layer2.tile(1, 3).solid_color(), Some(t));
        assert_eq!(layer2.tile(2, 3).solid_color(), Some(t));
    }

    #[test]
    fn test_fast_contract() {
        let mut layer = Layer::new(
            InternalLayerID(0),
            TILE_SIZE * 3,
            TILE_SIZE * 3,
            Tile::Blank,
        );
        // Change a pixel so the whole layer won't be of uniform color
        // and so we can distinguish the tiles from the new fully transparent ones.
        for y in 0..3 {
            for x in 0..3 {
                layer
                    .tile_mut(x, y)
                    .rect_iter_mut(0, &Rectangle::new(0, 0, 1, 1), false)
                    .next()
                    .unwrap()[0] = WHITE_PIXEL;
            }
        }

        let layer2 = layer.resized(-TILE_SIZEI, -TILE_SIZEI * 2, TILE_SIZEI, 0);

        // Should look like:
        // X
        // X
        // O

        assert_eq!(layer2.width(), TILE_SIZE);
        assert_eq!(layer2.height(), TILE_SIZE * 3);
        assert_eq!(layer2.tile(0, 0).solid_color(), None);
        assert_eq!(layer2.tile(0, 1).solid_color(), None);
        assert_eq!(layer2.tile(0, 2).solid_color(), Some(Color::TRANSPARENT));
    }

    #[test]
    fn test_slow_expand() {
        let mut layer = Layer::new(
            InternalLayerID(0),
            TILE_SIZE,
            TILE_SIZE,
            Tile::new(&Color::rgb8(0, 0, 0), 0),
        );
        layer
            .tile_mut(0, 0)
            .rect_iter_mut(0, &Rectangle::new(0, 0, 1, 1), false)
            .next()
            .unwrap()[0] = WHITE_PIXEL;

        let layer = layer.resized(10, 0, 0, 5);

        assert_eq!(layer.width, TILE_SIZE + 5);
        assert_eq!(layer.height, TILE_SIZE + 10);

        assert_eq!(layer.pixel_at(0, 0), ZERO_PIXEL);
        assert_eq!(layer.pixel_at(5, 10), WHITE_PIXEL);
    }

    #[test]
    fn test_slow_contract() {
        let mut layer = Layer::new(InternalLayerID(0), TILE_SIZE, TILE_SIZE, Tile::Blank);
        layer
            .tile_mut(0, 0)
            .rect_iter_mut(0, &Rectangle::new(5, 10, 1, 1), false)
            .next()
            .unwrap()[0] = WHITE_PIXEL;

        let layer = layer.resized(-10, 0, 0, -5);

        assert_eq!(layer.width, TILE_SIZE - 5);
        assert_eq!(layer.height, TILE_SIZE - 10);

        assert_eq!(layer.pixel_at(5, 10), ZERO_PIXEL);
        assert_eq!(layer.pixel_at(0, 0), WHITE_PIXEL);
    }

    #[test]
    fn test_sample_dab() {
        let mut layer = Layer::new(
            InternalLayerID(0),
            TILE_SIZE * 2,
            TILE_SIZE * 2,
            Tile::Blank,
        );
        layer.tile_mut(0, 0).fill(&Color::rgb8(255, 0, 0), 0);
        layer.tile_mut(1, 0).fill(&Color::rgb8(0, 255, 0), 0);
        layer.tile_mut(0, 1).fill(&Color::rgb8(0, 0, 255), 0);

        let dab_mask = BrushMask::new_round_pixel(32, 1.0);
        let sampled = layer.sample_dab_color(64 - 16, 64 - 16, &dab_mask);

        assert_eq!(
            sampled,
            Color {
                r: 0.333,
                g: 0.333,
                b: 0.333,
                a: 0.75
            }
        );
    }

    #[test]
    fn test_cropped_image() {
        let mut layer = Layer::new(
            InternalLayerID(0),
            TILE_SIZE * 4,
            TILE_SIZE * 4,
            Tile::Blank,
        );
        layer.tile_mut(1, 1).set_pixel_at(0, 0, 0, WHITE_PIXEL);
        layer.tile_mut(2, 1).set_pixel_at(0, 1, 0, WHITE_PIXEL);

        let (img, x, y) = layer.to_cropped_image();
        assert_eq!(x, TILE_SIZEI);
        assert_eq!(y, TILE_SIZEI);
        assert_eq!(img.width, TILE_SIZE as usize * 2);
        assert_eq!(img.height, TILE_SIZE as usize);
        assert_eq!(img.pixels.len(), (TILE_SIZE * 2 * TILE_SIZE) as usize);
        assert_eq!(img.pixels[0], WHITE_PIXEL);
        assert_eq!(img.pixels[TILE_SIZE as usize + 1], WHITE_PIXEL);
    }
}
