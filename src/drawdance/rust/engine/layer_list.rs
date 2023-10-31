// SPDX-License-Identifier: GPL-3.0-or-later
use super::{
    layer_content::LayerContent, Attached, AttachedLayerContent, AttachedLayerGroup, CArc,
    Detached, DetachedTransientLayerGroup, LayerGroup,
};
use crate::{
    DP_LayerList, DP_LayerListEntry, DP_TransientLayerList, DP_layer_list_at_noinc,
    DP_layer_list_content_at_noinc, DP_layer_list_count, DP_layer_list_decref,
    DP_layer_list_group_at_noinc, DP_layer_list_incref, DP_layer_list_transient,
    DP_transient_layer_list_decref, DP_transient_layer_list_incref,
    DP_transient_layer_list_new_init, DP_transient_layer_list_set_inc,
    DP_transient_layer_list_set_transient_group_noinc,
};
use std::ffi::c_int;

pub trait BaseLayerListEntry {
    fn persistent_ptr(&self) -> *mut DP_LayerListEntry;
}

pub struct LayerListEntry {
    data: *mut DP_LayerListEntry,
}

pub type AttachedLayerListEntry<'a, P> = Attached<'a, LayerListEntry, P>;

impl LayerListEntry {
    fn new_attached<P>(data: &mut DP_LayerListEntry) -> AttachedLayerListEntry<P> {
        Attached::new(LayerListEntry { data })
    }
}

impl<'a, T> BaseLayerListEntry for AttachedLayerListEntry<'a, T> {
    fn persistent_ptr(&self) -> *mut DP_LayerListEntry {
        self.data
    }
}

pub trait BaseLayerList {
    fn persistent_ptr(&self) -> *mut DP_LayerList;

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
        LayerListEntry::new_attached(unsafe { &mut *data })
    }

    fn content_at(&self, index: c_int) -> AttachedLayerContent<Self>
    where
        Self: Sized,
    {
        let data = unsafe { DP_layer_list_content_at_noinc(self.persistent_ptr(), index) };
        LayerContent::new_attached(unsafe { &mut *data })
    }

    fn group_at(&self, index: c_int) -> AttachedLayerGroup<Self>
    where
        Self: Sized,
    {
        let data = unsafe { DP_layer_list_group_at_noinc(self.persistent_ptr(), index) };
        LayerGroup::new_attached(unsafe { &mut *data })
    }
}

pub struct LayerList {
    data: *mut DP_LayerList,
}

pub type AttachedLayerList<'a, P> = Attached<'a, LayerList, P>;
pub type DetachedLayerList = Detached<DP_LayerList, LayerList>;

impl LayerList {
    pub fn new_attached<P>(data: &mut DP_LayerList) -> AttachedLayerList<P> {
        Attached::new(Self { data })
    }
}

impl BaseLayerList for LayerList {
    fn persistent_ptr(&self) -> *mut DP_LayerList {
        self.data
    }
}

impl CArc<DP_LayerList> for LayerList {
    unsafe fn incref(&mut self) {
        DP_layer_list_incref(self.data);
    }

    unsafe fn decref(&mut self) {
        DP_layer_list_decref(self.data);
    }

    fn as_mut_ptr(&mut self) -> *mut DP_LayerList {
        self.data
    }
}

pub struct TransientLayerList {
    data: *mut DP_TransientLayerList,
}

pub type AttachedTransientLayerList<'a, P> = Attached<'a, TransientLayerList, P>;
pub type DetachedTransientLayerList = Detached<DP_TransientLayerList, TransientLayerList>;

impl TransientLayerList {
    pub fn new_init(reserve: c_int) -> DetachedTransientLayerList {
        let data = unsafe { DP_transient_layer_list_new_init(reserve) };
        Detached::new_noinc(Self { data })
    }

    pub fn set_at_inc<T>(&mut self, lle: &AttachedLayerListEntry<T>, index: c_int) {
        unsafe {
            DP_transient_layer_list_set_inc(self.data, lle.persistent_ptr(), index);
        }
    }

    pub fn set_transient_group_at_noinc(&mut self, tlg: DetachedTransientLayerGroup, index: c_int) {
        unsafe {
            DP_transient_layer_list_set_transient_group_noinc(self.data, tlg.leak(), index);
        }
    }
}

impl BaseLayerList for TransientLayerList {
    fn persistent_ptr(&self) -> *mut DP_LayerList {
        self.data.cast()
    }
}

impl CArc<DP_TransientLayerList> for TransientLayerList {
    unsafe fn incref(&mut self) {
        DP_transient_layer_list_incref(self.data);
    }

    unsafe fn decref(&mut self) {
        DP_transient_layer_list_decref(self.data);
    }

    fn as_mut_ptr(&mut self) -> *mut DP_TransientLayerList {
        self.data
    }
}
