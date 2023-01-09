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

use crate::paint::Blendmode;
use crate::paint::Color;

/// cbindgen:field-names=[min, max]
#[derive(Copy, Clone)]
#[repr(C)]
pub struct Range(pub f32, pub f32);

#[derive(Copy, Clone)]
#[repr(u8)]
pub enum ClassicBrushShape {
    RoundPixel,
    SquarePixel,
    RoundSoft,
}

/// The parameters of a classic soft and pixel Drawpile brushes.
#[derive(Clone, Copy)]
#[repr(C)]
pub struct ClassicBrush {
    /// The diameter of the brush
    pub size: Range,

    /// Brush hardness (ignored for Pixel shapes)
    pub hardness: Range,

    /// Brush opacity (in indirect mode, this is the opacity of the entire stroke)
    pub opacity: Range,

    /// Smudging factor (1.0 means color is entirely replaced by picked up color)
    pub smudge: Range,

    /// Distance between dabs in diameters.
    pub spacing: f32,

    /// Color pickup frequency. <=1 means color is picked up at every dab.
    pub resmudge: i32,

    /// Initial brush color
    pub color: Color,

    /// Brush dab shape
    pub shape: ClassicBrushShape,

    /// Blending mode
    pub mode: Blendmode,

    /// Draw directly without using a temporary layer
    pub incremental: bool,

    /// Pick initial color from the layer
    pub colorpick: bool,

    /// Apply pressure to size
    pub size_pressure: bool,

    /// Apply pressure to hardness
    pub hardness_pressure: bool,

    /// Apply pressure to opacity
    pub opacity_pressure: bool,

    /// Apply pressure to smudging
    pub smudge_pressure: bool,
}

impl Default for ClassicBrush {
    fn default() -> Self {
        ClassicBrush {
            size: Range(1.0, 1.0),
            hardness: Range(0.0, 1.0),
            opacity: Range(0.0, 1.0),
            smudge: Range(0.0, 0.0),
            spacing: 0.1,
            resmudge: 0,
            color: Color::BLACK,
            shape: ClassicBrushShape::RoundPixel,
            mode: Blendmode::Normal,
            incremental: true,
            colorpick: false,
            size_pressure: false,
            hardness_pressure: false,
            opacity_pressure: false,
            smudge_pressure: false,
        }
    }
}

impl ClassicBrush {
    pub fn new() -> ClassicBrush {
        ClassicBrush::default()
    }

    pub fn is_pixelbrush(&self) -> bool {
        !matches!(self.shape, ClassicBrushShape::RoundSoft)
    }

    pub fn spacing_at(&self, p: f32) -> f32 {
        debug_assert!((0.0..=1.0).contains(&p));
        self.spacing * self.size_at(p)
    }

    pub fn size_at(&self, p: f32) -> f32 {
        debug_assert!((0.0..=1.0).contains(&p));
        if self.size_pressure {
            self.size.lerp(p)
        } else {
            self.size.1
        }
    }

    pub fn hardness_at(&self, p: f32) -> f32 {
        debug_assert!((0.0..=1.0).contains(&p));
        if self.hardness_pressure {
            self.hardness.lerp(p)
        } else {
            self.hardness.1
        }
    }

    pub fn opacity_at(&self, p: f32) -> f32 {
        debug_assert!((0.0..=1.0).contains(&p));
        if self.opacity_pressure {
            self.opacity.lerp(p)
        } else {
            self.opacity.1
        }
    }

    pub fn smudge_at(&self, p: f32) -> f32 {
        debug_assert!((0.0..=1.0).contains(&p));
        if self.smudge_pressure {
            self.smudge.lerp(p)
        } else {
            self.smudge.1
        }
    }
}

impl Range {
    pub fn lerp(self, alpha: f32) -> f32 {
        (self.1 - self.0) * alpha + self.0
    }
}

impl From<(f32, f32)> for Range {
    fn from(r: (f32, f32)) -> Self {
        Range(r.0, r.1)
    }
}
