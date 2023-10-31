// SPDX-License-Identifier: GPL-3.0-or-later
use super::{Attached, BaseTile, CArc, Detached, UPixels8};
use crate::{
    DP_LayerContent, DP_TransientLayerContent, DP_layer_content_decref, DP_layer_content_incref,
    DP_layer_content_to_upixels8, DP_layer_content_to_upixels8_cropped, DP_layer_content_transient,
    DP_transient_layer_content_decref, DP_transient_layer_content_incref,
    DP_transient_layer_content_new_init,
};
use std::{ffi::c_int, ptr::null_mut};

pub trait BaseLayerContent {
    fn persistent_ptr(&self) -> *mut DP_LayerContent;

    fn transient(&self) -> bool {
        unsafe { DP_layer_content_transient(self.persistent_ptr()) }
    }

    fn to_upixels8(&self, x: c_int, y: c_int, width: c_int, height: c_int) -> UPixels8 {
        let data =
            unsafe { DP_layer_content_to_upixels8(self.persistent_ptr(), x, y, width, height) };
        UPixels8::new(data, width as usize * height as usize)
    }

    fn to_upixels8_cropped(&self) -> (UPixels8, c_int, c_int, c_int, c_int) {
        let mut offset_x: c_int = 0;
        let mut offset_y: c_int = 0;
        let mut width: c_int = 0;
        let mut height: c_int = 0;
        let data = unsafe {
            DP_layer_content_to_upixels8_cropped(
                self.persistent_ptr(),
                &mut offset_x,
                &mut offset_y,
                &mut width,
                &mut height,
            )
        };
        (
            UPixels8::new(data, width as usize * height as usize),
            offset_x,
            offset_y,
            width,
            height,
        )
    }
}

pub struct LayerContent {
    data: *mut DP_LayerContent,
}

pub type AttachedLayerContent<'a, P> = Attached<'a, LayerContent, P>;
pub type DetachedLayerContent = Detached<DP_LayerContent, LayerContent>;

impl LayerContent {
    pub fn new_attached<P>(data: &mut DP_LayerContent) -> AttachedLayerContent<P> {
        Attached::new(Self { data })
    }
}

impl BaseLayerContent for LayerContent {
    fn persistent_ptr(&self) -> *mut DP_LayerContent {
        self.data
    }
}

impl CArc<DP_LayerContent> for LayerContent {
    unsafe fn incref(&mut self) {
        DP_layer_content_incref(self.data);
    }

    unsafe fn decref(&mut self) {
        DP_layer_content_decref(self.data);
    }

    fn as_mut_ptr(&mut self) -> *mut DP_LayerContent {
        self.data
    }
}

pub struct TransientLayerContent {
    data: *mut DP_TransientLayerContent,
}

pub type AttachedTransientLayerContent<'a, P> = Attached<'a, TransientLayerContent, P>;
pub type DetachedTransientLayerContent = Detached<DP_TransientLayerContent, TransientLayerContent>;

impl TransientLayerContent {
    pub fn new_init<T: BaseTile>(
        width: c_int,
        height: c_int,
        opt_tile: Option<&T>,
    ) -> DetachedTransientLayerContent {
        let t = opt_tile.map_or(null_mut(), BaseTile::persistent_ptr);
        let data = unsafe { DP_transient_layer_content_new_init(width, height, t) };
        Detached::new_noinc(Self { data })
    }
}

impl BaseLayerContent for TransientLayerContent {
    fn persistent_ptr(&self) -> *mut DP_LayerContent {
        self.data.cast()
    }
}

impl CArc<DP_TransientLayerContent> for TransientLayerContent {
    unsafe fn incref(&mut self) {
        DP_transient_layer_content_incref(self.data);
    }

    unsafe fn decref(&mut self) {
        DP_transient_layer_content_decref(self.data);
    }

    fn as_mut_ptr(&mut self) -> *mut DP_TransientLayerContent {
        self.data
    }
}
