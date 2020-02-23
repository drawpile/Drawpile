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

use super::tile::TILE_SIZEI;
use super::Rectangle;

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
    Resize(i32, i32),

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
    pub fn merge(self, other: AoE) -> Self {
        use AoE::*;
        match (self, other) {
            (Nothing, o) => o,
            (s, Nothing) => s,

            (Resize(sx, sy), Resize(ox, oy)) => Resize(sx + ox, sy + oy),
            (Resize(x, y), _) => Resize(x, y),
            (_, Resize(x, y)) => Resize(x, y),

            (Everything, _) => Everything,
            (_, Everything) => Everything,

            (Bitmap(mut s), Bitmap(o)) => {
                if s.w != o.w || s.h != o.h {
                    Resize(0, 0)
                } else {
                    s.tiles |= o.tiles;
                    Bitmap(s)
                }
            }
            (Bitmap(s), Bounds(r)) => s.set_tiles(&r).into(),
            (Bounds(r), Bitmap(o)) => o.set_tiles(&r).into(),
            (Bounds(r1), Bounds(r2)) => r1.union(&r2).into(),
        }
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
    pub fn new(w: u32, h: u32) -> TileMap {
        TileMap {
            tiles: bitvec![0;(w*h) as usize],
            w,
            h,
        }
    }

    fn set_tiles(mut self, r: &Rectangle) -> Self {
        let w = self.w as i32;
        let h = self.h as i32;
        if w == 0 || h == 0 {
            return self;
        }

        let x0 = bound(0, r.x / TILE_SIZEI, w - 1) as usize;
        let x1 = bound(0, r.right() / TILE_SIZEI, w - 1) as usize;
        let y0 = bound(0, r.y / TILE_SIZEI, h - 1) as usize;
        let y1 = bound(0, r.bottom() / TILE_SIZEI, h - 1) as usize;

        for y in y0..=y1 {
            self.tiles[y * self.w as usize + x0..=y * w as usize + x1].set_all(true);
        }

        self
    }

    /// Iterate through the changed tiles, returning the tile coordinates
    /// Returned tuple is: (linear tile index, tile x index, tile y index)
    pub fn iter_changed(&self) -> impl Iterator<Item = (usize, i32, i32)> + '_ {
        let w = self.w as usize;

        self.tiles
            .iter()
            .enumerate()
            .filter(|(_, b)| **b)
            .map(move |(e, _)| (e, (e % w) as i32, (e / w) as i32))
    }
}

fn bound(min: i32, v: i32, max: i32) -> i32 {
    v.max(min).min(max)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_bitmap_merge() {
        let mut a1 = TileMap::new(3, 3).set_tiles(&Rectangle::new(-70, -70, 140, 140));

        #[rustfmt::skip]
        assert_eq!(a1.tiles, bitvec![
            1, 1, 0,
            1, 1, 0,
            0, 0, 0,
        ]);

        let a2 = TileMap::new(3, 3).set_tiles(&Rectangle::new(120, 120, 200, 20));

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
    fn test_bitmap_iterator() {
        let a1 = TileMap::new(3, 3)
            .set_tiles(&Rectangle::new(0, 0, 1, 1))
            .set_tiles(&Rectangle::new(70, 70, 1, 1));

        let mut i = a1.iter_changed();
        assert_eq!(i.next(), Some((0, 0, 0)));
        assert_eq!(i.next(), Some((4, 1, 1)));
        assert_eq!(i.next(), None);
    }

    #[test]
    fn test_bitmap_conversion() {
        let tilemap = TileMap::new(3, 3).set_tiles(&Rectangle::new(-100, -100, 1000, 1000));
        let a: AoE = tilemap.into();

        assert_eq!(a, AoE::Everything);

        let tilemap = TileMap::new(3, 3);
        let a: AoE = tilemap.into();

        assert_eq!(a, AoE::Bitmap(TileMap::new(3, 3)));
    }
}
