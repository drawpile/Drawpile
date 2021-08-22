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

use super::aoe::{AoE, TileMap};
use super::layerstack::{LayerStack, LayerViewOptions};
use super::tile::{Tile, TileData, TILE_SIZE, TILE_SIZEI};

pub struct FlattenedTileIterator<'a> {
    layerstack: &'a LayerStack,
    opts: &'a LayerViewOptions,
    i: u32,
    j: u32,
    left: u32,
    right: u32,
    bottom: u32,
    tilemap: Option<TileMap>,
}

impl<'a> FlattenedTileIterator<'a> {
    pub fn new(layerstack: &'a LayerStack, opts: &'a LayerViewOptions, area: AoE) -> Self {
        let right = Tile::div_up(layerstack.width());
        let bottom = Tile::div_up(layerstack.height());
        match area {
            AoE::Resize(_, _, _) | AoE::Everything => FlattenedTileIterator {
                layerstack,
                opts,
                i: 0,
                j: 0,
                left: 0,
                right,
                bottom,
                tilemap: None,
            },
            AoE::Bitmap(tilemap) => FlattenedTileIterator {
                layerstack,
                opts,
                i: 0,
                j: 0,
                left: 0,
                right,
                bottom,
                tilemap: Some(tilemap),
            },
            AoE::Bounds(rect) => FlattenedTileIterator {
                layerstack,
                opts,
                i: rect.x as u32 / TILE_SIZE,
                j: rect.y as u32 / TILE_SIZE,
                left: rect.x as u32 / TILE_SIZE,
                right: Tile::div_up(rect.right() as u32),
                bottom: Tile::div_up(rect.bottom() as u32),
                tilemap: None,
            },
            AoE::Nothing => FlattenedTileIterator {
                layerstack,
                opts,
                i: 0,
                j: 1,
                left: 0,
                right: 0,
                bottom: 0,
                tilemap: None,
            },
        }
    }
}

impl<'a> Iterator for FlattenedTileIterator<'a> {
    type Item = (i32, i32, TileData);
    fn next(&mut self) -> Option<Self::Item> {
        loop {
            let (i, j) = (self.i, self.j);

            if j >= self.bottom {
                return None;
            }

            self.i += 1;
            if self.i >= self.right {
                self.i = self.left;
                self.j += 1;
            }

            if self.tilemap.as_ref().map_or(true, |tm| tm.is_set(i, j)) {
                return Some((
                    i as i32 * TILE_SIZEI,
                    j as i32 * TILE_SIZEI,
                    self.layerstack.flatten_tile(i, j, self.opts),
                ));
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::super::Rectangle;
    use super::*;

    #[test]
    fn test_everything() {
        let ls = LayerStack::new(100, 100);
        let opts = LayerViewOptions::default();
        let mut i = FlattenedTileIterator::new(&ls, &opts, AoE::Everything);

        let expected = [(0, 0), (64, 0), (0, 64), (64, 64)];

        for ex in &expected {
            let r = i.next().unwrap();
            assert_eq!((r.0, r.1), *ex);
        }
        assert!(i.next().is_none());
    }

    #[test]
    fn test_bounds() {
        let ls = LayerStack::new(1000, 1000);
        let opts = LayerViewOptions::default();
        let mut i =
            FlattenedTileIterator::new(&ls, &opts, AoE::Bounds(Rectangle::new(100, 100, 64, 64)));

        let expected = [(64, 64), (128, 64), (64, 128), (128, 128)];

        for ex in &expected {
            let r = i.next().unwrap();
            assert_eq!((r.0, r.1), *ex);
        }
        assert!(i.next().is_none());
    }

    #[test]
    fn test_tilemap() {
        let ls = LayerStack::new(5 * TILE_SIZE, 5 * TILE_SIZE);
        let opts = LayerViewOptions::default();
        let mut tm = TileMap::new(ls.width(), ls.height());
        tm.tiles.set(0, true);
        tm.tiles.set(6, true);
        tm.tiles.set(12, true);
        let mut i = FlattenedTileIterator::new(&ls, &opts, AoE::Bitmap(tm));

        let expected = [(0, 0), (64, 64), (128, 128)];

        for ex in &expected {
            let r = i.next().unwrap();
            assert_eq!((r.0, r.1), *ex);
        }
        assert!(i.next().is_none());
    }

    #[test]
    fn test_nothing() {
        let ls = LayerStack::new(100, 100);
        let opts = LayerViewOptions::default();
        let mut i = FlattenedTileIterator::new(&ls, &opts, AoE::Nothing);
        assert!(i.next().is_none());
    }
}
