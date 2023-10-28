use crate::{
    DP_LayerProps, DP_TransientLayerProps, DP_layer_props_children_noinc, DP_layer_props_id,
    DP_layer_props_title, DP_layer_props_transient, DP_transient_layer_props_decref,
    DP_transient_layer_props_id_set, DP_transient_layer_props_isolated_set,
    DP_transient_layer_props_new, DP_transient_layer_props_new_init_with_transient_children_noinc,
};
use std::{
    ffi::{c_char, c_int},
    marker::PhantomData,
    mem,
    slice::from_raw_parts,
};

use super::{BaseTransientLayerPropsList, TransientLayerPropsList};

// Base interface.

pub trait BaseLayerProps {
    fn persistent_ptr(&self) -> *mut DP_LayerProps;

    fn leak_persistent(self) -> *mut DP_LayerProps
    where
        Self: Sized,
    {
        let data = self.persistent_ptr();
        mem::forget(self);
        data
    }

    fn transient(&self) -> bool {
        unsafe { DP_layer_props_transient(self.persistent_ptr()) }
    }

    fn id(&self) -> c_int {
        unsafe { DP_layer_props_id(self.persistent_ptr()) }
    }

    fn title(&self) -> &[c_char] {
        let mut len = 0usize;
        let title = unsafe { DP_layer_props_title(self.persistent_ptr(), &mut len) };
        unsafe { from_raw_parts(title, len) }
    }

    fn is_group(&self) -> bool {
        !unsafe { DP_layer_props_children_noinc(self.persistent_ptr()) }.is_null()
    }
}

// Persistent marker.

pub trait BasePersistentLayerProps: BaseLayerProps {}

// Transient interface.

pub trait BaseTransientLayerProps: BaseLayerProps {
    fn transient_ptr(&mut self) -> *mut DP_TransientLayerProps;

    fn leak_transient(mut self) -> *mut DP_TransientLayerProps
    where
        Self: Sized,
    {
        let data = self.transient_ptr();
        mem::forget(self);
        data
    }

    fn set_id(&mut self, layer_id: c_int) {
        unsafe { DP_transient_layer_props_id_set(self.transient_ptr(), layer_id) }
    }

    fn set_isolated(&mut self, isolated: bool) {
        unsafe { DP_transient_layer_props_isolated_set(self.transient_ptr(), isolated) }
    }
}

// Attached persistent type, does not affect refcount.

pub struct AttachedLayerProps<'a, T> {
    data: *mut DP_LayerProps,
    phantom: PhantomData<&'a T>,
}

impl<'a, T> AttachedLayerProps<'a, T> {
    pub(super) fn new(data: *mut DP_LayerProps, _parent: &'a T) -> Self {
        Self {
            data,
            phantom: PhantomData,
        }
    }
}

impl<'a, T> BaseLayerProps for AttachedLayerProps<'a, T> {
    fn persistent_ptr(&self) -> *mut DP_LayerProps {
        self.data
    }
}

impl<'a, T> BasePersistentLayerProps for AttachedLayerProps<'a, T> {}

// Free transient type, affects refcount.

pub struct TransientLayerProps {
    data: *mut DP_TransientLayerProps,
}

impl TransientLayerProps {
    pub fn new<T: BasePersistentLayerProps>(lp: &T) -> Self {
        let data = unsafe { DP_transient_layer_props_new(lp.persistent_ptr()) };
        Self { data }
    }

    pub fn new_init_with_transient_children_noinc(
        layer_id: c_int,
        tlpl: TransientLayerPropsList,
    ) -> Self {
        let data = unsafe {
            DP_transient_layer_props_new_init_with_transient_children_noinc(
                layer_id,
                tlpl.leak_transient(),
            )
        };
        Self { data }
    }
}

impl BaseLayerProps for TransientLayerProps {
    fn persistent_ptr(&self) -> *mut DP_LayerProps {
        self.data.cast()
    }
}

impl BaseTransientLayerProps for TransientLayerProps {
    fn transient_ptr(&mut self) -> *mut DP_TransientLayerProps {
        self.data
    }
}

impl Drop for TransientLayerProps {
    fn drop(&mut self) {
        unsafe { DP_transient_layer_props_decref(self.data) }
    }
}
