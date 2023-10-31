// SPDX-License-Identifier: GPL-3.0-or-later
use super::{
    Attached, AttachedLayerProps, CArc, Detached, DetachedTransientLayerProps, LayerProps,
};
use crate::{
    DP_LayerPropsList, DP_TransientLayerPropsList, DP_layer_props_list_at_noinc,
    DP_layer_props_list_count, DP_layer_props_list_decref, DP_layer_props_list_incref,
    DP_layer_props_list_transient, DP_transient_layer_props_list_decref,
    DP_transient_layer_props_list_incref, DP_transient_layer_props_list_new_init,
    DP_transient_layer_props_list_set_transient_noinc,
};
use std::ffi::c_int;

pub trait BaseLayerPropsList {
    fn persistent_ptr(&self) -> *mut DP_LayerPropsList;

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
        LayerProps::new_attached(unsafe { &mut *data })
    }

    fn iter(&self) -> LayerPropsListIterator<Self>
    where
        Self: Sized,
    {
        LayerPropsListIterator::new(self)
    }
}

pub struct LayerPropsList {
    data: *mut DP_LayerPropsList,
}

pub type AttachedLayerPropsList<'a, P> = Attached<'a, LayerPropsList, P>;
pub type DetachedLayerPropsList = Detached<DP_LayerPropsList, LayerPropsList>;

impl LayerPropsList {
    pub fn new_attached<P>(data: &mut DP_LayerPropsList) -> AttachedLayerPropsList<P> {
        Attached::new(Self { data })
    }
}

impl BaseLayerPropsList for LayerPropsList {
    fn persistent_ptr(&self) -> *mut DP_LayerPropsList {
        self.data
    }
}

impl CArc<DP_LayerPropsList> for LayerPropsList {
    unsafe fn incref(&mut self) {
        DP_layer_props_list_incref(self.data);
    }

    unsafe fn decref(&mut self) {
        DP_layer_props_list_decref(self.data);
    }

    fn as_mut_ptr(&mut self) -> *mut DP_LayerPropsList {
        self.data
    }
}

pub struct TransientLayerPropsList {
    data: *mut DP_TransientLayerPropsList,
}

pub type AttachedTransientLayerPropsList<'a, P> = Attached<'a, TransientLayerPropsList, P>;
pub type DetachedTransientLayerPropsList =
    Detached<DP_TransientLayerPropsList, TransientLayerPropsList>;

impl TransientLayerPropsList {
    pub fn new_init(reserve: c_int) -> DetachedTransientLayerPropsList {
        let data = unsafe { DP_transient_layer_props_list_new_init(reserve) };
        Detached::new_noinc(Self { data })
    }

    pub fn set_transient_at_noinc(&mut self, tlp: DetachedTransientLayerProps, index: c_int) {
        unsafe {
            DP_transient_layer_props_list_set_transient_noinc(self.data, tlp.leak(), index);
        }
    }
}

impl BaseLayerPropsList for TransientLayerPropsList {
    fn persistent_ptr(&self) -> *mut DP_LayerPropsList {
        self.data.cast()
    }
}

impl CArc<DP_TransientLayerPropsList> for TransientLayerPropsList {
    unsafe fn incref(&mut self) {
        DP_transient_layer_props_list_incref(self.data);
    }

    unsafe fn decref(&mut self) {
        DP_transient_layer_props_list_decref(self.data);
    }

    fn as_mut_ptr(&mut self) -> *mut DP_TransientLayerPropsList {
        self.data
    }
}

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
