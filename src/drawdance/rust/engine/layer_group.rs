// SPDX-License-Identifier: GPL-3.0-or-later
use super::{Attached, AttachedLayerList, CArc, Detached, DetachedTransientLayerList, LayerList};
use crate::{
    DP_LayerGroup, DP_TransientLayerGroup, DP_layer_group_children_noinc, DP_layer_group_decref,
    DP_layer_group_incref, DP_layer_group_transient, DP_transient_layer_group_decref,
    DP_transient_layer_group_incref,
    DP_transient_layer_group_new_init_with_transient_children_noinc,
};
use std::ffi::c_int;

pub trait BaseLayerGroup {
    fn persistent_ptr(&self) -> *mut DP_LayerGroup;

    fn transient(&self) -> bool {
        unsafe { DP_layer_group_transient(self.persistent_ptr()) }
    }

    fn children(&self) -> AttachedLayerList<Self>
    where
        Self: Sized,
    {
        let data = unsafe { DP_layer_group_children_noinc(self.persistent_ptr()) };
        LayerList::new_attached(unsafe { &mut *data })
    }
}

pub struct LayerGroup {
    data: *mut DP_LayerGroup,
}

pub type AttachedLayerGroup<'a, P> = Attached<'a, LayerGroup, P>;
pub type DetachedLayerGroup = Detached<DP_LayerGroup, LayerGroup>;

impl LayerGroup {
    pub fn new_attached<P>(data: &mut DP_LayerGroup) -> AttachedLayerGroup<P> {
        Attached::new(Self { data })
    }
}

impl BaseLayerGroup for LayerGroup {
    fn persistent_ptr(&self) -> *mut DP_LayerGroup {
        self.data
    }
}

impl CArc<DP_LayerGroup> for LayerGroup {
    unsafe fn incref(&mut self) {
        DP_layer_group_incref(self.data);
    }

    unsafe fn decref(&mut self) {
        DP_layer_group_decref(self.data);
    }

    fn as_mut_ptr(&mut self) -> *mut DP_LayerGroup {
        self.data
    }
}

pub struct TransientLayerGroup {
    data: *mut DP_TransientLayerGroup,
}

pub type AttachedTransientLayerGroup<'a, P> = Attached<'a, TransientLayerGroup, P>;
pub type DetachedTransientLayerGroup = Detached<DP_TransientLayerGroup, TransientLayerGroup>;

impl TransientLayerGroup {
    pub fn new_init_with_transient_children_noinc(
        width: c_int,
        height: c_int,
        tll: DetachedTransientLayerList,
    ) -> DetachedTransientLayerGroup {
        let data = unsafe {
            DP_transient_layer_group_new_init_with_transient_children_noinc(
                width,
                height,
                tll.leak(),
            )
        };
        Detached::new_noinc(Self { data })
    }
}

impl BaseLayerGroup for TransientLayerGroup {
    fn persistent_ptr(&self) -> *mut DP_LayerGroup {
        self.data.cast()
    }
}

impl CArc<DP_TransientLayerGroup> for TransientLayerGroup {
    unsafe fn incref(&mut self) {
        DP_transient_layer_group_incref(self.data);
    }

    unsafe fn decref(&mut self) {
        DP_transient_layer_group_decref(self.data);
    }

    fn as_mut_ptr(&mut self) -> *mut DP_LayerGroup {
        self.data
    }
}
