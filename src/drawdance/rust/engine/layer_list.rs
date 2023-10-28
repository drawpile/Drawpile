// SPDX-License-Identifier: GPL-3.0-or-later
use crate::{
    DP_LayerList, DP_LayerListEntry, DP_TransientLayerList, DP_layer_list_at_noinc,
    DP_layer_list_count, DP_layer_list_transient, DP_transient_layer_list_decref,
    DP_transient_layer_list_new_init, DP_transient_layer_list_set_inc,
    DP_transient_layer_list_set_transient_group_noinc,
};
use std::{ffi::c_int, marker::PhantomData, mem};

use super::{BaseTransientLayerGroup, TransientLayerGroup};

// Base interface.

pub trait BaseLayerList {
    fn persistent_ptr(&self) -> *mut DP_LayerList;

    fn leak_persistent(self) -> *mut DP_LayerList
    where
        Self: Sized,
    {
        let data = self.persistent_ptr();
        mem::forget(self);
        data
    }

    fn transient(&self) -> bool {
        unsafe { DP_layer_list_transient(self.persistent_ptr()) }
    }

    fn count(&self) -> c_int {
        unsafe { DP_layer_list_count(self.persistent_ptr()) }
    }

    fn at(&self, index: c_int) -> AttachedLayerListEntry<Self>
    where
        Self: Sized,
    {
        let data = unsafe { DP_layer_list_at_noinc(self.persistent_ptr(), index) };
        AttachedLayerListEntry::new(data, self)
    }
}

// Persistent marker.

pub trait BasePersistentLayerList: BaseLayerList {}

// Transient interface.

pub trait BaseTransientLayerList: BaseLayerList {
    fn transient_ptr(&mut self) -> *mut DP_TransientLayerList;

    fn leak_transient(mut self) -> *mut DP_TransientLayerList
    where
        Self: Sized,
    {
        let data = self.transient_ptr();
        mem::forget(self);
        data
    }

    fn set_at_inc<T>(&mut self, lle: &AttachedLayerListEntry<T>, index: c_int) {
        unsafe {
            DP_transient_layer_list_set_inc(self.transient_ptr(), lle.persistent_ptr(), index);
        }
    }

    fn set_transient_group_at_noinc(&mut self, tlg: TransientLayerGroup, index: c_int) {
        unsafe {
            DP_transient_layer_list_set_transient_group_noinc(
                self.transient_ptr(),
                tlg.leak_transient(),
                index,
            );
        }
    }
}

// Attached persistent type, does not affect refcount.

pub struct AttachedLayerList<'a, T> {
    data: *mut DP_LayerList,
    phantom: PhantomData<&'a T>,
}

impl<'a, T> AttachedLayerList<'a, T> {
    pub(super) fn new(data: *mut DP_LayerList, _parent: &'a T) -> Self {
        Self {
            data,
            phantom: PhantomData,
        }
    }
}

impl<'a, T> BaseLayerList for AttachedLayerList<'a, T> {
    fn persistent_ptr(&self) -> *mut DP_LayerList {
        self.data
    }
}

impl<'a, T> BasePersistentLayerList for AttachedLayerList<'a, T> {}

// Free transient type, affects refcount.

pub struct TransientLayerList {
    data: *mut DP_TransientLayerList,
}

impl TransientLayerList {
    pub fn new_init(reserve: c_int) -> Self {
        let data = unsafe { DP_transient_layer_list_new_init(reserve) };
        Self { data }
    }
}

impl BaseLayerList for TransientLayerList {
    fn persistent_ptr(&self) -> *mut DP_LayerList {
        self.data.cast()
    }
}

impl BaseTransientLayerList for TransientLayerList {
    fn transient_ptr(&mut self) -> *mut DP_TransientLayerList {
        self.data
    }
}

impl Drop for TransientLayerList {
    fn drop(&mut self) {
        unsafe { DP_transient_layer_list_decref(self.data) }
    }
}

// Base entry interface.

pub trait BaseLayerListEntry {
    fn persistent_ptr(&self) -> *mut DP_LayerListEntry;

    fn leak_persistent(self) -> *mut DP_LayerListEntry
    where
        Self: Sized,
    {
        let data = self.persistent_ptr();
        mem::forget(self);
        data
    }
}

// Attached entry type, does not affect refcount.

pub struct AttachedLayerListEntry<'a, T> {
    data: *mut DP_LayerListEntry,
    phantom: PhantomData<&'a T>,
}

impl<'a, T> AttachedLayerListEntry<'a, T> {
    fn new(data: *mut DP_LayerListEntry, _parent: &T) -> Self {
        Self {
            data,
            phantom: PhantomData,
        }
    }
}

impl<'a, T> BaseLayerListEntry for AttachedLayerListEntry<'a, T> {
    fn persistent_ptr(&self) -> *mut DP_LayerListEntry {
        self.data
    }
}
