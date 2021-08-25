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

pub mod annotation;
pub mod aoe;
pub mod color;
pub mod editlayer;
pub mod floodfill;
pub mod layerstack;
pub mod rasterop;
pub mod rectiter;
pub mod tile;
pub mod tileiter;
pub mod tilevec;

use std::convert::TryFrom;
use std::fmt;

pub type UserID = u8;

/// Internal Layer ID is used on the layers themselves to support
/// the extended non-protocol range of IDs for temporary layers.
#[derive(Copy, Clone, PartialEq, Eq, Hash, Debug)]
#[repr(transparent)]
pub struct InternalLayerID(pub i32);

/// The regular layer ID type is used for layers that can be accessed
/// via the protocol.
pub type LayerID = u16;

impl fmt::Display for InternalLayerID {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{:04x}", self.0)
    }
}

impl PartialEq<LayerID> for InternalLayerID {
    fn eq(&self, other: &LayerID) -> bool {
        self.0 == *other as i32
    }
}

impl From<LayerID> for InternalLayerID {
    fn from(id: LayerID) -> Self {
        Self(id as i32)
    }
}

impl From<UserID> for InternalLayerID {
    fn from(id: UserID) -> Self {
        Self(id as i32)
    }
}

impl TryFrom<InternalLayerID> for LayerID {
    type Error = &'static str;

    fn try_from(id: InternalLayerID) -> Result<Self, Self::Error> {
        if id.0 < 0 || id.0 > 0xffff {
            Err("Non-internal layer IDs must be in range 0-65535")
        } else {
            Ok(id.0 as LayerID)
        }
    }
}

// Re-export types most commonly used from the outside
mod blendmode;
mod brushmask;
mod flattenediter;
mod image;
mod layer;
mod rect;

pub use self::image::Image;
pub use aoe::AoE;
pub use blendmode::Blendmode;
pub use brushmask::{BrushMask, ClassicBrushCache};
pub use color::{Color, Pixel};
pub use flattenediter::FlattenedTileIterator;
pub use layer::Layer;
pub use layerstack::{LayerStack, LayerViewMode, LayerViewOptions};
pub use rect::{Rectangle, Size};
pub use tile::Tile;
pub use tilevec::LayerTileSet;
