// SPDX-License-Identifier: GPL-3.0-or-later
use super::{Attached, Detached};
use crate::{DP_Tile, DP_tile_transient};

pub trait BaseTile {
    fn persistent_ptr(&self) -> *mut DP_Tile;

    fn transient(&self) -> bool {
        unsafe { DP_tile_transient(self.persistent_ptr()) }
    }
}

pub struct Tile {
    data: *mut DP_Tile,
}

pub type AttachedTile<'a, P> = Attached<'a, Tile, P>;
pub type DetachedTile = Detached<DP_Tile, Tile>;

impl Tile {
    pub fn new_attached<P>(data: &mut DP_Tile) -> AttachedTile<P> {
        Attached::new(Self { data })
    }

    pub fn new_attached_nullable<'a, P>(data: *mut DP_Tile) -> Option<AttachedTile<'a, P>> {
        unsafe { data.as_mut() }.map(Tile::new_attached)
    }
}

impl BaseTile for Tile {
    fn persistent_ptr(&self) -> *mut DP_Tile {
        self.data
    }
}
