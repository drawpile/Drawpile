// SPDX-License-Identifier: GPL-3.0-or-later
use super::{
    AttachedDocumentMetadata, AttachedLayerList, AttachedLayerPropsList, AttachedTile,
    AttachedTransientDocumentMetadata, BaseTile, BaseTransientLayerList,
    BaseTransientLayerPropsList, BaseTransientTimeline, TransientLayerList,
    TransientLayerPropsList, TransientTimeline,
};
use crate::{
    DP_CanvasState, DP_DrawContext, DP_TransientCanvasState, DP_canvas_state_background_opaque,
    DP_canvas_state_background_tile_noinc, DP_canvas_state_decref, DP_canvas_state_height,
    DP_canvas_state_incref, DP_canvas_state_layer_props_noinc, DP_canvas_state_layers_noinc,
    DP_canvas_state_metadata_noinc, DP_canvas_state_transient, DP_canvas_state_width,
    DP_tile_incref, DP_transient_canvas_state_background_tile_set_noinc,
    DP_transient_canvas_state_decref, DP_transient_canvas_state_height_set,
    DP_transient_canvas_state_layer_routes_reindex, DP_transient_canvas_state_new,
    DP_transient_canvas_state_persist, DP_transient_canvas_state_transient_layer_props_set_noinc,
    DP_transient_canvas_state_transient_layers_set_noinc,
    DP_transient_canvas_state_transient_metadata,
    DP_transient_canvas_state_transient_timeline_set_noinc, DP_transient_canvas_state_width_set,
};
use std::{ffi::c_int, mem, ptr::null_mut};

// Base interface.

pub trait BaseCanvasState {
    fn persistent_ptr(&self) -> *mut DP_CanvasState;

    fn leak_persistent(self) -> *mut DP_CanvasState
    where
        Self: Sized,
    {
        let data = self.persistent_ptr();
        mem::forget(self);
        data
    }

    fn transient(&self) -> bool {
        unsafe { DP_canvas_state_transient(self.persistent_ptr()) }
    }

    fn width(&self) -> c_int {
        unsafe { DP_canvas_state_width(self.persistent_ptr()) }
    }

    fn height(&self) -> c_int {
        unsafe { DP_canvas_state_height(self.persistent_ptr()) }
    }

    fn background_tile(&self) -> Option<AttachedTile<Self>>
    where
        Self: Sized,
    {
        let data = unsafe { DP_canvas_state_background_tile_noinc(self.persistent_ptr()) };
        if data.is_null() {
            None
        } else {
            Some(AttachedTile::new(data, self))
        }
    }

    fn background_opaque(&self) -> bool {
        unsafe { DP_canvas_state_background_opaque(self.persistent_ptr()) }
    }

    fn layers(&self) -> AttachedLayerList<Self>
    where
        Self: Sized,
    {
        let data = unsafe { DP_canvas_state_layers_noinc(self.persistent_ptr()) };
        AttachedLayerList::new(data, self)
    }

    fn layer_props(&self) -> AttachedLayerPropsList<Self>
    where
        Self: Sized,
    {
        let data = unsafe { DP_canvas_state_layer_props_noinc(self.persistent_ptr()) };
        AttachedLayerPropsList::new(data, self)
    }

    fn metadata(&self) -> AttachedDocumentMetadata<Self>
    where
        Self: Sized,
    {
        let data = unsafe { DP_canvas_state_metadata_noinc(self.persistent_ptr()) };
        AttachedDocumentMetadata::new(data, self)
    }
}

// Persistent marker.

pub trait BasePersistentCanvasState: BaseCanvasState {}

// Transient interface.

pub trait BaseTransientCanvasState: BaseCanvasState {
    fn transient_ptr(&mut self) -> *mut DP_TransientCanvasState;

    fn leak_transient(mut self) -> *mut DP_TransientCanvasState
    where
        Self: Sized,
    {
        let data = self.transient_ptr();
        mem::forget(self);
        data
    }

    fn persist(self) -> CanvasState
    where
        Self: Sized,
    {
        CanvasState::new_noinc(unsafe { DP_transient_canvas_state_persist(self.leak_transient()) })
    }

    fn transient_metadata(&mut self) -> AttachedTransientDocumentMetadata<Self>
    where
        Self: Sized,
    {
        let data = unsafe { DP_transient_canvas_state_transient_metadata(self.transient_ptr()) };
        AttachedTransientDocumentMetadata::new(data, self)
    }

    fn set_width(&mut self, width: c_int) {
        unsafe { DP_transient_canvas_state_width_set(self.transient_ptr(), width) }
    }

    fn set_height(&mut self, height: c_int) {
        unsafe { DP_transient_canvas_state_height_set(self.transient_ptr(), height) }
    }

    fn set_background_tile<T: BaseTile>(&mut self, opt_tile: &Option<T>, opaque: bool) {
        let t = match opt_tile {
            Some(tile) => unsafe { DP_tile_incref(tile.persistent_ptr()) },
            None => null_mut(),
        };
        unsafe {
            DP_transient_canvas_state_background_tile_set_noinc(self.transient_ptr(), t, opaque)
        }
    }

    fn set_transient_layers_noinc(&mut self, tll: TransientLayerList) {
        unsafe {
            DP_transient_canvas_state_transient_layers_set_noinc(
                self.transient_ptr(),
                tll.leak_transient(),
            )
        }
    }

    fn set_transient_layer_props_noinc(&mut self, tlpl: TransientLayerPropsList) {
        unsafe {
            DP_transient_canvas_state_transient_layer_props_set_noinc(
                self.transient_ptr(),
                tlpl.leak_transient(),
            )
        }
    }

    fn set_transient_timeline_noinc(&mut self, ttl: TransientTimeline) {
        unsafe {
            DP_transient_canvas_state_transient_timeline_set_noinc(
                self.transient_ptr(),
                ttl.leak_transient(),
            )
        }
    }

    fn reindex_layer_routes(&mut self, dc: *mut DP_DrawContext) {
        unsafe { DP_transient_canvas_state_layer_routes_reindex(self.transient_ptr(), dc) }
    }
}

// Free persistent type, affects refcount.

pub struct CanvasState {
    data: *mut DP_CanvasState,
}

impl CanvasState {
    pub fn new_noinc(data: *mut DP_CanvasState) -> Self {
        CanvasState { data }
    }

    pub fn new_inc(data: *mut DP_CanvasState) -> Self {
        Self::new_noinc(unsafe { DP_canvas_state_incref(data) })
    }

    pub fn new_noinc_nullable(data: *mut DP_CanvasState) -> Option<Self> {
        if data.is_null() {
            None
        } else {
            Some(Self::new_noinc(data))
        }
    }

    pub fn new_inc_nullable(data: *mut DP_CanvasState) -> Option<Self> {
        if data.is_null() {
            None
        } else {
            Some(Self::new_inc(data))
        }
    }
}

impl BaseCanvasState for CanvasState {
    fn persistent_ptr(&self) -> *mut DP_CanvasState {
        self.data
    }
}

impl BasePersistentCanvasState for CanvasState {}

impl Drop for CanvasState {
    fn drop(&mut self) {
        unsafe { DP_canvas_state_decref(self.data) }
    }
}

// Free transient type, affects refcount.

pub struct TransientCanvasState {
    data: *mut DP_TransientCanvasState,
}

impl TransientCanvasState {
    pub fn new<T: BasePersistentCanvasState>(cs: &T) -> Self {
        let data = unsafe { DP_transient_canvas_state_new(cs.persistent_ptr()) };
        Self { data }
    }
}

impl BaseCanvasState for TransientCanvasState {
    fn persistent_ptr(&self) -> *mut DP_CanvasState {
        self.data.cast()
    }
}

impl BaseTransientCanvasState for TransientCanvasState {
    fn transient_ptr(&mut self) -> *mut DP_TransientCanvasState {
        self.data
    }
}

impl Drop for TransientCanvasState {
    fn drop(&mut self) {
        unsafe { DP_transient_canvas_state_decref(self.data) }
    }
}
