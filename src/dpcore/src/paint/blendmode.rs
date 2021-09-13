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

use num_enum::IntoPrimitive;
use num_enum::TryFromPrimitive;

#[derive(Copy, Clone, Debug, PartialEq, IntoPrimitive, TryFromPrimitive)]
#[repr(u8)]
pub enum Blendmode {
    Erase = 0,
    Normal,
    Multiply,
    Divide,
    Burn,
    Dodge,
    Darken,
    Lighten,
    Subtract,
    Add,
    Recolor,
    Behind,
    ColorErase,
    Replace = 255,
}

impl Blendmode {
    pub fn can_decrease_opacity(self) -> bool {
        match self {
            Blendmode::Erase | Blendmode::ColorErase | Blendmode::Replace => true,
            _ => false,
        }
    }

    pub fn can_increase_opacity(self) -> bool {
        match self {
            Blendmode::Normal | Blendmode::Behind | Blendmode::Replace => true,
            _ => false,
        }
    }

    pub fn svg_name(self) -> &'static str {
        use Blendmode::*;
        match self {
            Erase => "-dp-erase",
            Normal => "svg:src-over",
            Multiply => "svg:multiply",
            Divide => "-dp-divide",
            Burn => "svg:color-burn",
            Dodge => "svg:color-dodge",
            Darken => "svg:darken",
            Lighten => "svg:lighten",
            Subtract => "-dp-minus", // not in SVG spec
            Add => "svg:plus",
            Recolor => "svg:src-atop",
            Behind => "svg:dst-over",
            ColorErase => "-dp-cerase",
            Replace => "-dp-replace",
        }
    }

    pub fn from_svg_name(name: &str) -> Option<Self> {
        let name = if name.starts_with("svg:") {
            &name[4..]
        } else {
            name
        };

        use Blendmode::*;
        Some(match name {
            "-dp-erase" => Erase,
            "src-over" => Normal,
            "multiply" => Multiply,
            "-dp-divide" => Divide,
            "color-burn" => Burn,
            "color-dodge" => Dodge,
            "darken" => Darken,
            "lighten" => Lighten,
            "-dp-minus" => Subtract,
            "plus" => Add,
            "src-atop" => Recolor,
            "dst-over" => Behind,
            "-dp-cerase" => ColorErase,
            "-dp-replace" => Replace,
            _ => {
                return None;
            }
        })
    }
}

impl Default for Blendmode {
    fn default() -> Self {
        Blendmode::Normal
    }
}
