// SPDX-License-Identifier: GPL-3.0-or-later
use super::{
    types::Persistable, Attached, AttachedDocumentMetadata, AttachedLayerList,
    AttachedLayerPropsList, AttachedTile, AttachedTransientDocumentMetadata, BaseTile, CArc,
    Detached, DetachedTransientLayerList, DetachedTransientLayerPropsList,
    DetachedTransientTimeline, DocumentMetadata, LayerList, LayerPropsList, Tile,
    TransientDocumentMetadata,
};
use crate::{
    dp_error_anyhow, DP_CanvasState, DP_DrawContext, DP_TransientCanvasState,
    DP_canvas_state_background_opaque, DP_canvas_state_background_tile_noinc,
    DP_canvas_state_decref, DP_canvas_state_height, DP_canvas_state_incref,
    DP_canvas_state_layer_props_noinc, DP_canvas_state_layers_noinc,
    DP_canvas_state_metadata_noinc, DP_canvas_state_to_flat_separated_urgba8,
    DP_canvas_state_transient, DP_canvas_state_width, DP_tile_incref,
    DP_transient_canvas_state_background_tile_set_noinc, DP_transient_canvas_state_decref,
    DP_transient_canvas_state_height_set, DP_transient_canvas_state_incref,
    DP_transient_canvas_state_layer_routes_reindex, DP_transient_canvas_state_new,
    DP_transient_canvas_state_persist, DP_transient_canvas_state_transient_layer_props_set_noinc,
    DP_transient_canvas_state_transient_layers_set_noinc,
    DP_transient_canvas_state_transient_metadata,
    DP_transient_canvas_state_transient_timeline_set_noinc, DP_transient_canvas_state_width_set,
    DP_FLAT_IMAGE_RENDER_FLAGS,
};
use anyhow::Result;
use std::{
    ffi::c_int,
    ptr::{null, null_mut},
};

pub trait BaseCanvasState {
    fn persistent_ptr(&self) -> *mut DP_CanvasState;

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
        Tile::new_attached_nullable(data)
    }

    fn background_opaque(&self) -> bool {
        unsafe { DP_canvas_state_background_opaque(self.persistent_ptr()) }
    }

    fn layers(&self) -> AttachedLayerList<Self>
    where
        Self: Sized,
    {
        let data = unsafe { DP_canvas_state_layers_noinc(self.persistent_ptr()) };
        LayerList::new_attached(unsafe { &mut *data })
    }

    fn layer_props(&self) -> AttachedLayerPropsList<Self>
    where
        Self: Sized,
    {
        let data = unsafe { DP_canvas_state_layer_props_noinc(self.persistent_ptr()) };
        LayerPropsList::new_attached(unsafe { &mut *data })
    }

    fn metadata(&self) -> AttachedDocumentMetadata<Self>
    where
        Self: Sized,
    {
        let data = unsafe { DP_canvas_state_metadata_noinc(self.persistent_ptr()) };
        DocumentMetadata::new_attached(unsafe { &mut *data })
    }

    fn to_flat_separated_urgba8(&self) -> Result<Vec<u8>> {
        let mut buffer = vec![0_u8; self.width() as usize * self.height() as usize * 4];
        let ok = unsafe {
            DP_canvas_state_to_flat_separated_urgba8(
                self.persistent_ptr(),
                DP_FLAT_IMAGE_RENDER_FLAGS,
                null(),
                null(),
                buffer.as_mut_ptr(),
            )
        };
        if ok {
            Ok(buffer)
        } else {
            Err(dp_error_anyhow())
        }
    }
}

pub struct CanvasState {
    data: *mut DP_CanvasState,
}

pub type AttachedCanvasState<'a, P> = Attached<'a, CanvasState, P>;
pub type DetachedCanvasState = Detached<DP_CanvasState, CanvasState>;

impl CanvasState {
    pub fn new_attached(data: &mut DP_CanvasState) -> AttachedCanvasState<()> {
        Attached::new(Self { data })
    }

    pub fn new_detached_inc(data: &mut DP_CanvasState) -> DetachedCanvasState {
        Detached::new_inc(Self { data })
    }

    pub fn new_detached_noinc(data: &mut DP_CanvasState) -> DetachedCanvasState {
        Detached::new_noinc(Self { data })
    }

    pub fn new_detached_inc_nullable(data: *mut DP_CanvasState) -> Option<DetachedCanvasState> {
        unsafe { data.as_mut() }.map(Self::new_detached_inc)
    }

    pub fn new_detached_noinc_nullable(data: *mut DP_CanvasState) -> Option<DetachedCanvasState> {
        unsafe { data.as_mut() }.map(Self::new_detached_noinc)
    }
}

impl BaseCanvasState for CanvasState {
    fn persistent_ptr(&self) -> *mut DP_CanvasState {
        self.data
    }
}

impl CArc<DP_CanvasState> for CanvasState {
    unsafe fn incref(&mut self) {
        DP_canvas_state_incref(self.data);
    }

    unsafe fn decref(&mut self) {
        DP_canvas_state_decref(self.data);
    }

    fn as_mut_ptr(&mut self) -> *mut DP_CanvasState {
        self.data
    }
}

pub struct TransientCanvasState {
    data: *mut DP_TransientCanvasState,
}

pub type AttachedTransientCanvasState<'a, P> = Attached<'a, TransientCanvasState, P>;
pub type DetachedTransientCanvasState = Detached<DP_TransientCanvasState, TransientCanvasState>;

impl TransientCanvasState {
    pub fn new(cs: &CanvasState) -> DetachedTransientCanvasState {
        let data = unsafe { DP_transient_canvas_state_new(cs.persistent_ptr()) };
        Detached::new_noinc(Self { data })
    }

    pub fn transient_metadata(&mut self) -> AttachedTransientDocumentMetadata<Self> {
        let data = unsafe { DP_transient_canvas_state_transient_metadata(self.data) };
        TransientDocumentMetadata::new_attached(unsafe { &mut *data })
    }

    pub fn set_width(&mut self, width: c_int) {
        unsafe { DP_transient_canvas_state_width_set(self.data, width) }
    }

    pub fn set_height(&mut self, height: c_int) {
        unsafe { DP_transient_canvas_state_height_set(self.data, height) }
    }

    pub fn set_background_tile<T: BaseTile>(&mut self, opt_tile: &Option<T>, opaque: bool) {
        let t = match opt_tile {
            Some(tile) => unsafe { DP_tile_incref(tile.persistent_ptr()) },
            None => null_mut(),
        };
        unsafe {
            DP_transient_canvas_state_background_tile_set_noinc(self.data, t, opaque);
        }
    }

    pub fn set_transient_layers_noinc(&mut self, tll: DetachedTransientLayerList) {
        unsafe {
            DP_transient_canvas_state_transient_layers_set_noinc(self.data, tll.leak());
        }
    }

    pub fn set_transient_layer_props_noinc(&mut self, tlpl: DetachedTransientLayerPropsList) {
        unsafe {
            DP_transient_canvas_state_transient_layer_props_set_noinc(self.data, tlpl.leak());
        }
    }

    pub fn set_transient_timeline_noinc(&mut self, ttl: DetachedTransientTimeline) {
        unsafe {
            DP_transient_canvas_state_transient_timeline_set_noinc(self.data, ttl.leak());
        }
    }

    pub fn reindex_layer_routes(&mut self, dc: *mut DP_DrawContext) {
        unsafe { DP_transient_canvas_state_layer_routes_reindex(self.data, dc) }
    }
}

impl BaseCanvasState for TransientCanvasState {
    fn persistent_ptr(&self) -> *mut DP_CanvasState {
        self.data.cast()
    }
}

impl CArc<DP_TransientCanvasState> for TransientCanvasState {
    unsafe fn incref(&mut self) {
        DP_transient_canvas_state_incref(self.data);
    }

    unsafe fn decref(&mut self) {
        DP_transient_canvas_state_decref(self.data);
    }

    fn as_mut_ptr(&mut self) -> *mut DP_TransientCanvasState {
        self.data
    }
}

impl Persistable<DP_CanvasState, CanvasState> for TransientCanvasState {
    unsafe fn persist(&mut self) -> DetachedCanvasState {
        let data = DP_transient_canvas_state_persist(self.data);
        CanvasState::new_detached_noinc(&mut *data)
    }
}
