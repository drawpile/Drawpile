use super::{
    Attached, AttachedLayerPropsList, CArc, Detached, DetachedTransientLayerPropsList,
    LayerPropsList,
};
use crate::{
    DP_BlendMode, DP_LayerProps, DP_TransientLayerProps, DP_channel15_to_8,
    DP_layer_props_blend_mode, DP_layer_props_censored, DP_layer_props_children_noinc,
    DP_layer_props_decref, DP_layer_props_hidden, DP_layer_props_id, DP_layer_props_incref,
    DP_layer_props_isolated, DP_layer_props_opacity, DP_layer_props_title,
    DP_layer_props_transient, DP_transient_layer_props_decref, DP_transient_layer_props_id_set,
    DP_transient_layer_props_incref, DP_transient_layer_props_isolated_set,
    DP_transient_layer_props_new, DP_transient_layer_props_new_init_with_transient_children_noinc,
};
use std::{
    ffi::{c_char, c_int},
    slice::from_raw_parts,
};

pub trait BaseLayerProps {
    fn persistent_ptr(&self) -> *mut DP_LayerProps;

    fn transient(&self) -> bool {
        unsafe { DP_layer_props_transient(self.persistent_ptr()) }
    }

    fn id(&self) -> c_int {
        unsafe { DP_layer_props_id(self.persistent_ptr()) }
    }

    fn title(&self) -> &[c_char] {
        let mut len = 0_usize;
        let title = unsafe { DP_layer_props_title(self.persistent_ptr(), &mut len) };
        unsafe { from_raw_parts(title, len) }
    }

    fn title_u8(&self) -> &[u8] {
        let mut len = 0_usize;
        let title = unsafe { DP_layer_props_title(self.persistent_ptr(), &mut len) };
        unsafe { from_raw_parts(title.cast(), len) }
    }

    fn opacity(&self) -> u16 {
        unsafe { DP_layer_props_opacity(self.persistent_ptr()) }
    }

    fn opacity_u8(&self) -> u8 {
        unsafe { DP_channel15_to_8(self.opacity()) }
    }

    fn blend_mode(&self) -> DP_BlendMode {
        unsafe { DP_layer_props_blend_mode(self.persistent_ptr()) as DP_BlendMode }
    }

    fn hidden(&self) -> bool {
        unsafe { DP_layer_props_hidden(self.persistent_ptr()) }
    }

    fn censored(&self) -> bool {
        unsafe { DP_layer_props_censored(self.persistent_ptr()) }
    }

    fn isolated(&self) -> bool {
        unsafe { DP_layer_props_isolated(self.persistent_ptr()) }
    }

    fn is_group(&self) -> bool {
        !unsafe { DP_layer_props_children_noinc(self.persistent_ptr()) }.is_null()
    }

    fn children(&self) -> Option<AttachedLayerPropsList<Self>>
    where
        Self: Sized,
    {
        let data = unsafe { DP_layer_props_children_noinc(self.persistent_ptr()) };
        if data.is_null() {
            None
        } else {
            Some(LayerPropsList::new_attached(unsafe { &mut *data }))
        }
    }
}

pub struct LayerProps {
    data: *mut DP_LayerProps,
}

pub type AttachedLayerProps<'a, P> = Attached<'a, LayerProps, P>;
pub type DetachedLayerProps = Detached<DP_LayerProps, LayerProps>;

impl LayerProps {
    pub fn new_attached<P>(data: &mut DP_LayerProps) -> AttachedLayerProps<P> {
        Attached::new(Self { data })
    }
}

impl BaseLayerProps for LayerProps {
    fn persistent_ptr(&self) -> *mut DP_LayerProps {
        self.data
    }
}

impl CArc<DP_LayerProps> for LayerProps {
    unsafe fn incref(&mut self) {
        DP_layer_props_incref(self.data);
    }

    unsafe fn decref(&mut self) {
        DP_layer_props_decref(self.data);
    }

    fn as_mut_ptr(&mut self) -> *mut DP_LayerProps {
        self.data
    }
}

pub struct TransientLayerProps {
    data: *mut DP_TransientLayerProps,
}

pub type AttachedTransientLayerProps<'a, P> = Attached<'a, TransientLayerProps, P>;
pub type DetachedTransientLayerProps = Detached<DP_TransientLayerProps, TransientLayerProps>;

impl TransientLayerProps {
    pub fn new(lp: &LayerProps) -> DetachedTransientLayerProps {
        let data = unsafe { DP_transient_layer_props_new(lp.data) };
        Detached::new_noinc(Self { data })
    }

    pub fn new_init_with_transient_children_noinc(
        layer_id: c_int,
        tlpl: DetachedTransientLayerPropsList,
    ) -> DetachedTransientLayerProps {
        let data = unsafe {
            DP_transient_layer_props_new_init_with_transient_children_noinc(layer_id, tlpl.leak())
        };
        Detached::new_noinc(Self { data })
    }

    pub fn transient_ptr(&mut self) -> *mut DP_TransientLayerProps {
        self.data
    }

    pub fn set_id(&mut self, layer_id: c_int) {
        unsafe { DP_transient_layer_props_id_set(self.data, layer_id) }
    }

    pub fn set_isolated(&mut self, isolated: bool) {
        unsafe { DP_transient_layer_props_isolated_set(self.data, isolated) }
    }
}

impl BaseLayerProps for TransientLayerProps {
    fn persistent_ptr(&self) -> *mut DP_LayerProps {
        self.data.cast()
    }
}

impl CArc<DP_TransientLayerProps> for TransientLayerProps {
    unsafe fn incref(&mut self) {
        DP_transient_layer_props_incref(self.data);
    }

    unsafe fn decref(&mut self) {
        DP_transient_layer_props_decref(self.data);
    }

    fn as_mut_ptr(&mut self) -> *mut DP_TransientLayerProps {
        self.data
    }
}
