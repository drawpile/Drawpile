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

use lazy_static::lazy_static;
use std::fmt;
use std::sync::Arc;

use super::color::*;
use super::rectiter::{MutableRectIterator, RectIterator};
use super::{rasterop, Blendmode, Rectangle, UserID};

pub const TILE_SIZE: u32 = 64;
pub const TILE_SIZEI: i32 = TILE_SIZE as i32;
pub const TILE_LENGTH: usize = (TILE_SIZE * TILE_SIZE) as usize;

lazy_static! {
    pub(super) static ref ZEBRA_TILE: TileData = {
        let mut z = TileData::new(Color::TRANSPARENT.as_pixel15(), 0);
        let colors = [
            Color::rgb8(35, 38, 41).as_pixel15(),
            Color::rgb8(239, 240, 241).as_pixel15(),
        ];

        z.pixels
            .iter_mut()
            .enumerate()
            .for_each(|(i, p)| *p = colors[(i / 64 + i % 64) / 16 % colors.len()]);
        z
    };
}

#[derive(Clone)]
pub struct TileData {
    /// The pixel content
    pub pixels: [Pixel15; TILE_LENGTH],

    /// ID of the user who last touched this tile
    pub last_touched_by: UserID,

    /// This tile has been touched by an operation that can decrease
    /// pixel opacity since the last time optimize() was called.
    /// May be eligible for turning into a Tile::Blank.
    /// Setting this to true is always safe and should be done if in doubt.
    maybe_blank: bool,
}

#[derive(Clone)]
pub enum Tile {
    Bitmap(Arc<TileData>),
    Blank,
}

static TRANSPARENT_DATA: TileData = TileData {
    pixels: [ZERO_PIXEL15; TILE_LENGTH],
    last_touched_by: 0,
    maybe_blank: true,
};

impl TileData {
    pub fn new(pixel: Pixel15, user: UserID) -> TileData {
        TileData {
            pixels: [pixel; TILE_LENGTH],
            last_touched_by: user,
            maybe_blank: pixel == ZERO_PIXEL15,
        }
    }

    pub fn merge_data(&mut self, other: &TileData, opacity: f32, mode: Blendmode) {
        rasterop::pixel_blend(
            &mut self.pixels,
            &other.pixels,
            (opacity * BIT15_F32) as u16,
            mode,
        );
        self.maybe_blank = mode.can_decrease_opacity();
        self.last_touched_by = other.last_touched_by;
    }

    pub fn merge_tile(&mut self, other: &Tile, opacity: f32, mode: Blendmode) {
        match other {
            Tile::Bitmap(td) => self.merge_data(td, opacity, mode),
            Tile::Blank => (),
        }
    }
}

impl Tile {
    // Construct a new tile filled with the given color.
    // If the color is transparent, a Blank tile is returned.
    pub fn new(color: &Color, user: UserID) -> Tile {
        let p = color.as_pixel15();
        if p[ALPHA_CHANNEL] == 0 {
            Tile::Blank
        } else {
            Tile::Bitmap(Arc::new(TileData::new(p, user)))
        }
    }

    // Construct a new tile filled with the given color.
    // A bitmap tile is constructed even if the color is transparent.
    pub fn new_solid(color: &Color, user: UserID) -> Tile {
        Tile::Bitmap(Arc::new(TileData::new(color.as_pixel15(), user)))
    }

    pub fn new_checkerboard(color1: &Color, color2: &Color) -> Tile {
        let mut td = TileData::new(color1.as_pixel15(), 0);
        let p2 = color2.as_pixel15();
        let s = TILE_SIZE as usize;
        let s2 = TILE_SIZE as usize / 2;
        for y in 0..s2 {
            td.pixels[y * s..y * s + s2].fill(p2);
            td.pixels[(y + s2) * s + s2..(y + s2) * s + s].fill(p2);
        }

        Tile::Bitmap(Arc::new(td))
    }

    pub fn from_data(data: &[Pixel8], user: UserID) -> Tile {
        assert_eq!(data.len(), TILE_LENGTH, "Wrong tile data length");
        let mut td = Arc::new(TileData::new(ZERO_PIXEL15, user));
        pixels8_to_15(&mut Arc::make_mut(&mut td).pixels, data);
        Tile::Bitmap(td)
    }

    pub fn div_up(x: u32) -> u32 {
        (x + TILE_SIZE - 1) / TILE_SIZE
    }

    // Check if every pixel of this tile is the same
    pub fn solid_color(&self) -> Option<Color> {
        match self {
            Tile::Bitmap(td) => {
                let pix = td.pixels[0];
                if td.pixels.iter().all(|&p| p == pix) {
                    Some(Color::from_pixel15(pix))
                } else {
                    None
                }
            }
            Tile::Blank => Some(Color::TRANSPARENT),
        }
    }

    pub fn clone_data(&self) -> TileData {
        match self {
            Tile::Bitmap(td) => TileData::clone(td),
            Tile::Blank => TileData::new(ZERO_PIXEL15, 0),
        }
    }

    pub fn data(&self) -> &TileData {
        match self {
            Tile::Bitmap(td) => td,
            Tile::Blank => &TRANSPARENT_DATA,
        }
    }

    /// Check if all pixels of this tile are fully transparent.
    /// Shortcircuits if maybe_blank is false.
    pub fn is_blank(&self) -> bool {
        match self {
            Tile::Bitmap(td) => td.maybe_blank && td.pixels.iter().all(|&p| p[ALPHA_CHANNEL] == 0),
            Tile::Blank => true,
        }
    }

    /// Convert this to a Tile::Blank tile if it's blank.
    /// The maybe_blank flag is cleared if the tile isn't really blank.
    pub fn optimize(&mut self) {
        match self {
            Tile::Bitmap(td) => {
                if td.maybe_blank {
                    if td.pixels.iter().all(|&p| p[ALPHA_CHANNEL] == 0) {
                        *self = Tile::Blank;
                    } else {
                        Arc::make_mut(td).maybe_blank = false;
                    }
                }
            }
            Tile::Blank => (),
        }
    }

    /// Get the last touched by user ID tag
    pub fn last_touched_by(&self) -> UserID {
        match self {
            Tile::Bitmap(td) => td.last_touched_by,
            Tile::Blank => 0,
        }
    }

    // Fill this tile with a solid color
    pub fn fill(&mut self, color: &Color, user: UserID) {
        if color.is_transparent() {
            *self = Tile::Blank
        } else {
            match self {
                Tile::Bitmap(td) => {
                    let pixel = color.as_pixel15();
                    let data = Arc::make_mut(td);
                    data.last_touched_by = user;
                    data.maybe_blank = false;
                    for i in data.pixels.iter_mut() {
                        *i = pixel;
                    }
                }
                Tile::Blank => {
                    *self = Tile::new_solid(color, user);
                }
            }
        }
    }

    /// Merge this tile with the other tile
    pub fn merge(&mut self, other: &Tile, opacity: f32, mode: Blendmode) {
        if let Tile::Bitmap(o) = other {
            match self {
                Tile::Bitmap(td) => Arc::make_mut(td).merge_data(o, opacity, mode),
                Tile::Blank => {
                    if mode.can_increase_opacity() {
                        if opacity == 1.0 {
                            *self = other.clone();
                        } else {
                            *self = Tile::new_solid(&Color::TRANSPARENT, other.last_touched_by());
                            self.merge(other, opacity, mode);
                        }
                    }
                }
            }
        }
    }

    // Return a rect iterator to this tile's content
    // Note: you may want to check if this is a Blank tile first for optimization purposes
    pub fn rect_iter(&self, r: &Rectangle) -> RectIterator<Pixel15> {
        debug_assert!(r.x >= 0 && r.y >= 0);
        debug_assert!(r.right() < TILE_SIZEI && r.bottom() < TILE_SIZEI);

        match self {
            Tile::Bitmap(d) => RectIterator::from_rectangle(&d.pixels, TILE_SIZE as usize, r),
            Tile::Blank => {
                RectIterator::from_rectangle(&TRANSPARENT_DATA.pixels, TILE_SIZE as usize, r)
            }
        }
    }

    /// Return a mutable iterator to this tile's content
    /// If this is a Blank tile, it is converted to a fully transparent Bitmap tile first.
    /// If you intend to apply any operation that can decrease pixel opacity, set maybe_erase to
    /// true. Otherwise, is_blank may return false even if the tile is actually transparent.
    pub fn rect_iter_mut(
        &mut self,
        user: UserID,
        r: &Rectangle,
        maybe_erase: bool,
    ) -> MutableRectIterator<Pixel15> {
        debug_assert!(r.x >= 0 && r.y >= 0);
        debug_assert!(r.right() < TILE_SIZEI && r.bottom() < TILE_SIZEI);

        match self {
            Tile::Bitmap(td) => {
                let data = Arc::make_mut(td);
                data.maybe_blank |= maybe_erase;
                MutableRectIterator::from_rectangle(&mut data.pixels, TILE_SIZE as usize, r)
            }
            Tile::Blank => {
                *self = Tile::new_solid(&Color::TRANSPARENT, user);
                self.rect_iter_mut(user, r, true)
            }
        }
    }

    pub fn pixel_at(&self, x: u32, y: u32) -> Pixel15 {
        debug_assert!(x < TILE_SIZE);
        debug_assert!(y < TILE_SIZE);
        match self {
            Tile::Bitmap(td) => td.pixels[(y * TILE_SIZE + x) as usize],
            Tile::Blank => ZERO_PIXEL15,
        }
    }

    pub fn set_pixel_at(&mut self, user: UserID, x: u32, y: u32, px: Pixel15) {
        debug_assert!(x < TILE_SIZE);
        debug_assert!(y < TILE_SIZE);
        match self {
            Tile::Bitmap(td) => {
                let data = Arc::make_mut(td);
                data.pixels[(y * TILE_SIZE + x) as usize] = px;
                data.maybe_blank |= px == ZERO_PIXEL15;
            }
            Tile::Blank => {
                *self = Tile::new_solid(&Color::TRANSPARENT, user);
                self.set_pixel_at(user, x, y, px);
            }
        }
    }

    /// Do a shallow equality comparison between these two tiles
    pub fn ptr_eq(&self, other: &Tile) -> bool {
        use Tile::*;
        match (self, other) {
            (Blank, Blank) => true,
            (Bitmap(a), Bitmap(b)) => Arc::ptr_eq(a, b),
            (_, _) => false,
        }
    }

    #[cfg(test)]
    pub fn refcount(&self) -> usize {
        match self {
            Tile::Bitmap(d) => Arc::strong_count(d) + Arc::weak_count(d),
            Tile::Blank => 0,
        }
    }

    #[cfg(debug_assertions)]
    pub fn to_ascii_art(&self) -> String {
        let mut art = String::new();
        match self {
            Tile::Bitmap(td) => {
                for y in 0..TILE_SIZE {
                    for x in 0..TILE_SIZE {
                        art.push(if td.pixels[(y * TILE_SIZE + x) as usize][0] == 0 {
                            '.'
                        } else {
                            'X'
                        });
                    }
                    art.push('\n');
                }
            }
            Tile::Blank => {
                art = String::from("[BLANK TILE]");
            }
        }
        art
    }
}

impl PartialEq for Tile {
    fn eq(&self, other: &Tile) -> bool {
        self.ptr_eq(other)
            || match self {
                Tile::Bitmap(td) => match other {
                    Tile::Bitmap(otd) => td.pixels[..] == otd.pixels[..],
                    Tile::Blank => self.is_blank(),
                },
                Tile::Blank => other.is_blank(),
            }
    }
}

impl fmt::Debug for Tile {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Tile::Bitmap(d) => write!(
                f,
                "Tile(pixels=[{:?}...{:?}], user={}, refs={})",
                d.pixels[0],
                d.pixels[TILE_LENGTH - 1],
                d.last_touched_by,
                Arc::strong_count(d)
            ),
            Tile::Blank => write!(f, "Tile(blank)"),
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_cow() {
        let mut tile = Tile::new_solid(&Color::TRANSPARENT, 0);
        let tile2 = tile.clone();
        let tile3 = tile2.clone();

        assert_eq!(tile.refcount(), 3);
        assert_eq!(tile2.refcount(), 3);
        assert_eq!(tile3.refcount(), 3);

        tile.fill(
            &Color {
                r: 1.0,
                g: 0.0,
                b: 0.0,
                a: 1.0,
            },
            1,
        );

        assert_eq!(tile.refcount(), 1);
        assert_eq!(tile2.refcount(), 2);
        assert_eq!(tile3.refcount(), 2);
    }

    #[test]
    fn test_cow_vec() {
        let mut vec1 = vec![Tile::new_solid(&Color::TRANSPARENT, 0); 3];
        let vec2 = vec1.clone();

        assert_eq!(vec1[0].refcount(), 6);
        vec1[0] = Tile::Blank;
        assert_eq!(vec2[0].refcount(), 5);
    }

    #[test]
    fn test_solid() {
        let mut tile = Tile::Blank;
        assert_eq!(tile.solid_color(), Some(Color::TRANSPARENT));
        assert!(tile.is_blank());

        let red = Color {
            r: 1.0,
            g: 0.0,
            b: 0.0,
            a: 1.0,
        };
        tile.fill(&red, 1);
        assert_eq!(tile.solid_color(), Some(red));
        assert!(!tile.is_blank());

        tile.rect_iter_mut(1, &Rectangle::new(0, 0, 3, 3), false)
            .flatten()
            .for_each(|p| *p = WHITE_PIXEL15);
        assert_eq!(tile.solid_color(), None);
        assert!(!tile.is_blank());
    }

    #[test]
    fn test_merge() {
        let mut btm = Tile::new_solid(&Color::rgb8(0, 0, 0), 0);
        let top = Tile::new_solid(&Color::rgb8(255, 255, 255), 0);
        btm.merge(&top, 0.5, Blendmode::Normal);

        assert_eq!(
            btm.solid_color(),
            Some(Color {
                r: 0.5,
                g: 0.5,
                b: 0.5,
                a: 1.0
            })
        );
    }

    #[test]
    fn test_merge_blank() {
        let mut btm = Tile::Blank;
        let top = Tile::new_solid(&Color::rgb8(255, 255, 255), 0);
        btm.merge(&top, 0.5, Blendmode::Normal);

        assert_eq!(
            btm.solid_color(),
            Some(Color {
                r: 1.0,
                g: 1.0,
                b: 1.0,
                a: 0.5
            })
        );
    }

    #[test]
    fn test_pixels() {
        let mut t = Tile::Blank;
        t.set_pixel_at(0, 0, 0, WHITE_PIXEL15);
        assert_eq!(t.pixel_at(0, 0), WHITE_PIXEL15);
        assert_eq!(t.pixel_at(1, 0), ZERO_PIXEL15);
    }
}
