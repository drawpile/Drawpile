// This file is part of Drawpile.
// Copyright (C) 2020-2022 Calle Laakkonen
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

use super::tile::{Tile, TILE_SIZEI};
use super::{Rectangle, Size};
use core::ops::Not;
use std::cmp::{max, min};

use bitvec::prelude::*;

/// A layer editing operation's area of effect.
///
/// This is used inform layer stack observers which part
/// of the screen or cache pixmap should be refreshed.
///
/// The AoE is not specific to any layer, since a change typically means
/// the entire stack for that region (or to be exact, the tiles intersecting
/// that region) should be re-rendered. Resizing the canvas typically invalidates
/// the entire cache, since a new pixmap must be allocated.
///
/// Note that the AoE can be `Nothing` even if some pixels were changed,
/// if the changed layer is hidden.
#[derive(Debug, PartialEq, Clone)]
pub enum AoE {
    /// Canvas was resized.
    /// This typically should be interpreted the same way as Everything.
    /// The shifts indicate how much the existing content moved.
    /// The size is the previous size of the canvas
    Resize(i32, i32, Size),

    /// Content (but not the size) of the entire canvas has changed
    Everything,

    /// A set of changed tiles
    Bitmap(TileMap),

    /// A rectangular region has changed
    Bounds(Rectangle),

    /// No visible changes
    Nothing,
}

impl AoE {
    /// Merge two areas together
    pub fn merge(self, other: AoE) -> Self {
        use AoE::*;
        match (self, other) {
            (Nothing, o) => o,
            (s, Nothing) => s,

            (Resize(sx, sy, ss), Resize(ox, oy, _)) => Resize(sx + ox, sy + oy, ss),
            (Resize(x, y, s), _) => Resize(x, y, s),
            (_, Resize(x, y, s)) => Resize(x, y, s),

            (Everything, _) => Everything,
            (_, Everything) => Everything,

            (Bitmap(mut s), Bitmap(o)) => {
                if s.w != o.w || s.h != o.h {
                    Resize(
                        0,
                        0,
                        Size::new(s.w as i32 * TILE_SIZEI, s.h as i32 * TILE_SIZEI),
                    )
                } else {
                    s.tiles |= o.tiles;
                    Bitmap(s)
                }
            }
            (Bitmap(s), Bounds(r)) => s.set_rect(&r, true).into(),
            (Bounds(r), Bitmap(o)) => o.set_rect(&r, true).into(),
            (Bounds(r1), Bounds(r2)) => r1.union(&r2).into(),
        }
    }

    /// Return the difference and intersection of the given rectangle and this one
    ///
    /// This is used to do partial updates of a view cache. The difference is the area
    /// that remains dirty and the intersection is the area that should be updated.
    pub fn diff_and_intersect(self, rect: Rectangle, bounds: Size) -> (Self, Self) {
        use AoE::*;
        match self {
            Nothing => (Nothing, Nothing),

            Everything | Resize(_, _, _) => (
                TileMap::new_set(bounds.width as u32, bounds.height as u32)
                    .set_rect(&rect, false)
                    .into(),
                rect.into(),
            ),

            Bounds(s) => {
                if rect.contains(&s) {
                    return (Nothing, s.into());
                }

                let i = s.intersected(&rect);
                match i {
                    // Rectangle intersection can produce a non-convex shape so we just turn
                    // everything into bitmaps here
                    Some(intersection) => (
                        TileMap::new(bounds.width as u32, bounds.height as u32)
                            .set_rect(&s, true)
                            .set_rect(&intersection, false)
                            .into(),
                        intersection.into(),
                    ),
                    None => (s.into(), Nothing),
                }
            }

            Bitmap(s) => {
                let rect_bits =
                    TileMap::new(bounds.width as u32, bounds.height as u32).set_rect(&rect, true);
                let intersection = s.clone().intersected(&rect_bits);
                (s.clear_tiles(rect_bits).into(), intersection.into())
            }
        }
    }

    pub fn bounds(&self, bounds: Size) -> Option<Rectangle> {
        use AoE::*;
        match self {
            Nothing => None,
            Everything | Resize(_, _, _) => {
                if bounds.width <= 0 || bounds.height <= 0 {
                    None
                } else {
                    Some(Rectangle::new(0, 0, bounds.width, bounds.height))
                }
            }
            Bounds(b) => Some(*b),
            Bitmap(b) => b.bounds(),
        }
    }

    pub fn is_nothing(&self) -> bool {
        matches!(self, Self::Nothing)
    }
}

impl From<TileMap> for AoE {
    fn from(item: TileMap) -> AoE {
        if item.tiles.all() {
            AoE::Everything
        } else {
            AoE::Bitmap(item)
        }
    }
}

impl From<Rectangle> for AoE {
    fn from(item: Rectangle) -> AoE {
        AoE::Bounds(item)
    }
}

#[derive(Debug, PartialEq, Clone)]
pub struct TileMap {
    /// Bitmap of changed tiles
    pub tiles: BitVec,
    /// Width in tiles
    pub w: u32,
    /// Height in tiles
    pub h: u32,
}

impl TileMap {
    /// Construct a new blank tilemap
    ///
    /// Dimensions are given in pixels
    pub fn new(w: u32, h: u32) -> TileMap {
        let w = Tile::div_up(w);
        let h = Tile::div_up(h);
        TileMap {
            tiles: bitvec![0;(w*h) as usize],
            w,
            h,
        }
    }

    /// Construct a tilemap where all tiles are set to 1
    ///
    /// Dimensions are given in pixels
    pub fn new_set(w: u32, h: u32) -> TileMap {
        let w = Tile::div_up(w);
        let h = Tile::div_up(h);
        TileMap {
            tiles: bitvec![1;(w*h) as usize],
            w,
            h,
        }
    }

    pub fn is_set(&self, i: u32, j: u32) -> bool {
        self.tiles[(j * self.w + i) as usize]
    }

    pub fn intersected(mut self, other: &TileMap) -> Self {
        assert!(self.w == other.w && self.h == other.h);
        self.tiles &= other.tiles.clone();
        self
    }

    fn set_rect(mut self, r: &Rectangle, value: bool) -> Self {
        let w = self.w as i32;
        let h = self.h as i32;
        if w == 0 || h == 0 {
            return self;
        }

        let x0 = (r.x / TILE_SIZEI).clamp(0, w - 1) as usize;
        let x1 = (r.right() / TILE_SIZEI).clamp(0, w - 1) as usize;
        let y0 = (r.y / TILE_SIZEI).clamp(0, h - 1) as usize;
        let y1 = (r.bottom() / TILE_SIZEI).clamp(0, h - 1) as usize;

        for y in y0..=y1 {
            self.tiles[y * self.w as usize + x0..=y * w as usize + x1].fill(value);
        }

        self
    }

    fn clear_tiles(mut self, tm: TileMap) -> Self {
        assert!(self.w == tm.w && self.h == tm.h);
        self.tiles &= !tm.tiles;
        self
    }

    /// Iterate through the changed tiles, returning the tile coordinates
    /// Returned tuple is: (linear tile index, tile x index, tile y index)
    pub fn iter_changed(&self) -> impl Iterator<Item = (usize, i32, i32)> + '_ {
        let w = self.w as usize;

        self.tiles
            .iter_ones()
            .map(move |i| (i, (i % w) as i32, (i / w) as i32))
    }

    pub fn bounds(&self) -> Option<Rectangle> {
        let mut left = self.w as i32;
        let mut right = 0i32;
        let mut top = self.h as i32;
        let mut bottom = 0i32;

        for (row, bits) in self.tiles.chunks(self.w as usize).enumerate() {
            if let Some(first) = bits.first_one() {
                left = min(left, first as i32);
                right = max(right, bits.last_one().unwrap() as i32);
                top = min(top, row as i32);
                bottom = max(bottom, row as i32);
            }
        }
        if left < self.w as i32 {
            Some(Rectangle::new(
                left * TILE_SIZEI,
                top * TILE_SIZEI,
                (right - left + 1) * TILE_SIZEI,
                (bottom - top + 1) * TILE_SIZEI,
            ))
        } else {
            None
        }
    }
}

impl Not for TileMap {
    type Output = Self;

    fn not(self) -> Self::Output {
        TileMap {
            tiles: !self.tiles,
            w: self.w,
            h: self.h,
        }
    }
}

#[cfg(test)]
mod tests {
    use super::super::tile::TILE_SIZE;
    use super::*;

    #[test]
    fn test_bitmap_merge() {
        let mut a1 = TileMap::new(3 * TILE_SIZE, 3 * TILE_SIZE)
            .set_rect(&Rectangle::new(-70, -70, 140, 140), true);

        #[rustfmt::skip]
        assert_eq!(a1.tiles, bitvec![
            1, 1, 0,
            1, 1, 0,
            0, 0, 0,
        ]);

        let a2 = TileMap::new(3 * TILE_SIZE, 3 * TILE_SIZE)
            .set_rect(&Rectangle::new(120, 120, 200, 20), true);

        #[rustfmt::skip]
        assert_eq!(a2.tiles, bitvec![
            0, 0, 0,
            0, 1, 1,
            0, 1, 1,
        ]);

        a1.tiles |= a2.tiles;
        #[rustfmt::skip]
        assert_eq!(a1.tiles, bitvec![
            1, 1, 0,
            1, 1, 1,
            0, 1, 1,
        ]);
    }

    #[test]
    fn test_bitmap_clear() {
        let wh = 3 * TILE_SIZE;
        let a1 = TileMap::new(wh, wh)
            .set_rect(&Rectangle::new(0, 0, TILE_SIZEI * 2, TILE_SIZEI * 2), true)
            .clear_tiles(TileMap::new(wh, wh).set_rect(
                &Rectangle::new(TILE_SIZEI, TILE_SIZEI, TILE_SIZEI * 2, TILE_SIZEI * 2),
                true,
            ));

        #[rustfmt::skip]
        assert_eq!(a1.tiles, bitvec![
            1, 1, 0,
            1, 0, 0,
            0, 0, 0,
        ]);
    }

    #[test]
    fn test_bitmap_iterator() {
        let a1 = TileMap::new(3 * TILE_SIZE, 3 * TILE_SIZE)
            .set_rect(&Rectangle::new(0, 0, 1, 1), true)
            .set_rect(&Rectangle::new(70, 70, 1, 1), true);

        let mut i = a1.iter_changed();
        assert_eq!(i.next(), Some((0, 0, 0)));
        assert_eq!(i.next(), Some((4, 1, 1)));
        assert_eq!(i.next(), None);
    }

    #[test]
    fn test_bitmap_conversion() {
        let tilemap = TileMap::new(3 * TILE_SIZE, 3 * TILE_SIZE)
            .set_rect(&Rectangle::new(-100, -100, 1000, 1000), true);
        let a: AoE = tilemap.into();

        assert_eq!(a, AoE::Everything);

        let tilemap = TileMap::new(3 * TILE_SIZE, 3 * TILE_SIZE);
        let a: AoE = tilemap.into();

        assert_eq!(a, AoE::Bitmap(TileMap::new(3 * TILE_SIZE, 3 * TILE_SIZE)));
    }
}
