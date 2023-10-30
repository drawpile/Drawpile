// SPDX-License-Identifier: GPL-3.0-or-later
use crate::{
    DP_LayerGroup, DP_TransientLayerGroup, DP_layer_group_children_noinc, DP_layer_group_transient,
    DP_transient_layer_group_decref,
    DP_transient_layer_group_new_init_with_transient_children_noinc,
};
use std::{ffi::c_int, marker::PhantomData, mem};

use super::{AttachedLayerList, BaseTransientLayerList, TransientLayerList};

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

    fn children(&self) -> AttachedLayerList<Self>
    where
        Self: Sized,
    {
        let data = unsafe { DP_layer_group_children_noinc(self.persistent_ptr()) };
        AttachedLayerList::new(data)
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

// Attached persistent type, does not affect refcount.

pub struct AttachedLayerGroup<'a, T: ?Sized> {
    data: *mut DP_LayerGroup,
    phantom: PhantomData<&'a T>,
}

impl<'a, T: ?Sized> AttachedLayerGroup<'a, T> {
    pub(super) fn new(data: *mut DP_LayerGroup) -> Self {
        debug_assert!(!data.is_null());
        Self {
            data,
            phantom: PhantomData,
        }
    }
}

impl<'a, T: ?Sized> BaseLayerGroup for AttachedLayerGroup<'a, T> {
    fn persistent_ptr(&self) -> *mut DP_LayerGroup {
        self.data
    }
}

impl<'a, T: ?Sized> BasePersistentLayerGroup for AttachedLayerGroup<'a, T> {}

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
