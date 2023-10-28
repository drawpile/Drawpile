// SPDX-License-Identifier: GPL-3.0-or-later
use crate::{DP_Tile, DP_tile_transient};
use std::{marker::PhantomData, mem};

// Base interface.

pub trait BaseTile {
    fn persistent_ptr(&self) -> *mut DP_Tile;

    fn leak_persistent(self) -> *mut DP_Tile
    where
        Self: Sized,
    {
        let data = self.persistent_ptr();
        mem::forget(self);
        data
    }

    fn transient(&self) -> bool {
        unsafe { DP_tile_transient(self.persistent_ptr()) }
    }
}

// Persistent marker.

pub trait BasePersistentTile: BaseTile {}

// Attached persistent type, does not affect refcount.

pub struct AttachedTile<'a, T> {
    data: *mut DP_Tile,
    phantom: PhantomData<&'a T>,
}

impl<'a, T> AttachedTile<'a, T> {
    pub(super) fn new(data: *mut DP_Tile, _parent: &'a T) -> Self {
        Self {
            data,
            phantom: PhantomData,
        }
    }
}

impl<'a, T> BaseTile for AttachedTile<'a, T> {
    fn persistent_ptr(&self) -> *mut DP_Tile {
        self.data
    }
}

impl<'a, T> BasePersistentTile for AttachedTile<'a, T> {}
