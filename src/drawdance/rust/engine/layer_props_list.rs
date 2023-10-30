// SPDX-License-Identifier: GPL-3.0-or-later
use super::{AttachedLayerProps, BaseTransientLayerProps, TransientLayerProps};
use crate::{
    DP_LayerPropsList, DP_TransientLayerPropsList, DP_layer_props_list_at_noinc,
    DP_layer_props_list_count, DP_layer_props_list_transient, DP_transient_layer_props_list_decref,
    DP_transient_layer_props_list_new_init, DP_transient_layer_props_list_set_transient_noinc,
};
use std::{ffi::c_int, marker::PhantomData, mem};

// Base interface.

pub trait BaseLayerPropsList {
    fn persistent_ptr(&self) -> *mut DP_LayerPropsList;

    fn leak_persistent(self) -> *mut DP_LayerPropsList
    where
        Self: Sized,
    {
        let data = self.persistent_ptr();
        mem::forget(self);
        data
    }

    fn transient(&self) -> bool {
        unsafe { DP_layer_props_list_transient(self.persistent_ptr()) }
    }

    fn count(&self) -> c_int {
        unsafe { DP_layer_props_list_count(self.persistent_ptr()) }
    }

    fn at(&self, index: c_int) -> AttachedLayerProps<Self>
    where
        Self: Sized,
    {
        let data = unsafe { DP_layer_props_list_at_noinc(self.persistent_ptr(), index) };
        AttachedLayerProps::new(data)
    }

    fn dyn_at(&self, index: c_int) -> AttachedLayerProps<dyn BaseLayerPropsList> {
        let data = unsafe { DP_layer_props_list_at_noinc(self.persistent_ptr(), index) };
        AttachedLayerProps::new(data)
    }

    fn iter(&self) -> LayerPropsListIterator<Self>
    where
        Self: Sized,
    {
        LayerPropsListIterator::new(self)
    }
}

// Persistent marker.

pub trait BasePersistentLayerPropsList: BaseLayerPropsList {}

// Transient interface.

pub trait BaseTransientLayerPropsList: BaseLayerPropsList {
    fn transient_ptr(&mut self) -> *mut DP_TransientLayerPropsList;

    fn leak_transient(mut self) -> *mut DP_TransientLayerPropsList
    where
        Self: Sized,
    {
        let data = self.transient_ptr();
        mem::forget(self);
        data
    }

    fn set_transient_at_noinc(&mut self, tlp: TransientLayerProps, index: c_int) {
        unsafe {
            DP_transient_layer_props_list_set_transient_noinc(
                self.transient_ptr(),
                tlp.leak_transient(),
                index,
            );
        }
    }
}

// Attached persistent type, does not affect refcount.

pub struct AttachedLayerPropsList<'a, T: ?Sized> {
    data: *mut DP_LayerPropsList,
    phantom: PhantomData<&'a T>,
}

impl<'a, T: ?Sized> AttachedLayerPropsList<'a, T> {
    pub(super) fn new(data: *mut DP_LayerPropsList) -> Self {
        debug_assert!(!data.is_null());
        Self {
            data,
            phantom: PhantomData,
        }
    }
}

impl<'a, T: ?Sized> BaseLayerPropsList for AttachedLayerPropsList<'a, T> {
    fn persistent_ptr(&self) -> *mut DP_LayerPropsList {
        self.data
    }
}

impl<'a, T: ?Sized> BasePersistentLayerPropsList for AttachedLayerPropsList<'a, T> {}

// Free transient type, affects refcount.

pub struct TransientLayerPropsList {
    data: *mut DP_TransientLayerPropsList,
}

impl TransientLayerPropsList {
    pub fn new_init(reserve: c_int) -> Self {
        let data = unsafe { DP_transient_layer_props_list_new_init(reserve) };
        Self { data }
    }
}

impl BaseLayerPropsList for TransientLayerPropsList {
    fn persistent_ptr(&self) -> *mut DP_LayerPropsList {
        self.data.cast()
    }
}

impl BaseTransientLayerPropsList for TransientLayerPropsList {
    fn transient_ptr(&mut self) -> *mut DP_TransientLayerPropsList {
        self.data
    }
}

impl Drop for TransientLayerPropsList {
    fn drop(&mut self) {
        unsafe { DP_transient_layer_props_list_decref(self.data) }
    }
}

// Persistent iterator.

pub struct LayerPropsListIterator<'a, T: BaseLayerPropsList> {
    parent: &'a T,
    index: c_int,
    count: c_int,
}

impl<'a, T: BaseLayerPropsList> LayerPropsListIterator<'a, T> {
    fn new(parent: &'a T) -> Self {
        Self {
            parent,
            index: 0,
            count: parent.count(),
        }
    }
}

impl<'a, T: BaseLayerPropsList> Iterator for LayerPropsListIterator<'a, T> {
    type Item = AttachedLayerProps<'a, T>;

    fn next(&mut self) -> Option<Self::Item> {
        if self.index < self.count {
            let lp = self.parent.at(self.index);
            self.index += 1;
            Some(lp)
        } else {
            None
        }
    }
}
