// SPDX-License-Identifier: GPL-3.0-or-later
use crate::{
    DP_LayerGroup, DP_TransientLayerGroup, DP_layer_group_transient,
    DP_transient_layer_group_decref,
    DP_transient_layer_group_new_init_with_transient_children_noinc,
};
use std::{ffi::c_int, mem};

use super::{BaseTransientLayerList, TransientLayerList};

// Base interface.

pub trait BaseLayerGroup {
    fn persistent_ptr(&self) -> *mut DP_LayerGroup;

    fn leak_persistent(self) -> *mut DP_LayerGroup
    where
        Self: Sized,
    {
        let data = self.persistent_ptr();
        mem::forget(self);
        data
    }

    fn transient(&self) -> bool {
        unsafe { DP_layer_group_transient(self.persistent_ptr()) }
    }
}

// Persistent marker.

pub trait BasePersistentLayerGroup: BaseLayerGroup {}

// Transient interface.

pub trait BaseTransientLayerGroup: BaseLayerGroup {
    fn transient_ptr(&mut self) -> *mut DP_TransientLayerGroup;

    fn leak_transient(mut self) -> *mut DP_TransientLayerGroup
    where
        Self: Sized,
    {
        let data = self.transient_ptr();
        mem::forget(self);
        data
    }
}

// Free transient type, affects refcount.

pub struct TransientLayerGroup {
    data: *mut DP_TransientLayerGroup,
}

impl TransientLayerGroup {
    pub fn new_init_with_transient_children_noinc(
        width: c_int,
        height: c_int,
        tll: TransientLayerList,
    ) -> Self {
        let data = unsafe {
            DP_transient_layer_group_new_init_with_transient_children_noinc(
                width,
                height,
                tll.leak_transient(),
            )
        };
        TransientLayerGroup { data }
    }
}

impl BaseLayerGroup for TransientLayerGroup {
    fn persistent_ptr(&self) -> *mut DP_LayerGroup {
        self.data.cast()
    }
}

impl BaseTransientLayerGroup for TransientLayerGroup {
    fn transient_ptr(&mut self) -> *mut DP_TransientLayerGroup {
        self.data
    }
}

impl Drop for TransientLayerGroup {
    fn drop(&mut self) {
        unsafe { DP_transient_layer_group_decref(self.data) }
    }
}
