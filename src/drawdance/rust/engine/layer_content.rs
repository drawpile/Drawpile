// SPDX-License-Identifier: GPL-3.0-or-later
use super::{BaseTile, UPixels8};
use crate::{
    DP_LayerContent, DP_TransientLayerContent, DP_layer_content_to_upixels8,
    DP_layer_content_to_upixels8_cropped, DP_layer_content_transient,
    DP_transient_layer_content_decref, DP_transient_layer_content_new_init,
};
use std::{ffi::c_int, marker::PhantomData, mem, ptr::null_mut};

// Base interface.

pub trait BaseLayerContent {
    fn persistent_ptr(&self) -> *mut DP_LayerContent;

    fn leak_persistent(self) -> *mut DP_LayerContent
    where
        Self: Sized,
    {
        let data = self.persistent_ptr();
        mem::forget(self);
        data
    }

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

// Persistent marker.

pub trait BasePersistentLayerContent: BaseLayerContent {}

// Attached persistent type, does not affect refcount.

pub struct AttachedLayerContent<'a, T: ?Sized> {
    data: *mut DP_LayerContent,
    phantom: PhantomData<&'a T>,
}

impl<'a, T: ?Sized> AttachedLayerContent<'a, T> {
    pub(super) fn new(data: *mut DP_LayerContent) -> Self {
        debug_assert!(!data.is_null());
        Self {
            data,
            phantom: PhantomData,
        }
    }
}

impl<'a, T: ?Sized> BaseLayerContent for AttachedLayerContent<'a, T> {
    fn persistent_ptr(&self) -> *mut DP_LayerContent {
        self.data
    }
}

impl<'a, T: ?Sized> BasePersistentLayerContent for AttachedLayerContent<'a, T> {}

// Transient interface.

pub trait BaseTransientLayerContent: BaseLayerContent {
    fn transient_ptr(&mut self) -> *mut DP_TransientLayerContent;

    fn leak_transient(mut self) -> *mut DP_TransientLayerContent
    where
        Self: Sized,
    {
        let data = self.transient_ptr();
        mem::forget(self);
        data
    }
}

// Free transient type, affects refcount.

pub struct TransientLayerContent {
    data: *mut DP_TransientLayerContent,
}

impl TransientLayerContent {
    pub fn new_init<T: BaseTile>(width: c_int, height: c_int, opt_tile: Option<&T>) -> Self {
        let t = opt_tile.map_or(null_mut(), BaseTile::persistent_ptr);
        let data = unsafe { DP_transient_layer_content_new_init(width, height, t) };
        TransientLayerContent { data }
    }
}

impl BaseLayerContent for TransientLayerContent {
    fn persistent_ptr(&self) -> *mut DP_LayerContent {
        self.data.cast()
    }
}

impl BaseTransientLayerContent for TransientLayerContent {
    fn transient_ptr(&mut self) -> *mut DP_TransientLayerContent {
        self.data
    }
}

impl Drop for TransientLayerContent {
    fn drop(&mut self) {
        unsafe { DP_transient_layer_content_decref(self.data) }
    }
}
