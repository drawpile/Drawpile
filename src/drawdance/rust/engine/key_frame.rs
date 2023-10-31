// SPDX-License-Identifier: GPL-3.0-or-later
use crate::{
    DP_KeyFrame, DP_TransientKeyFrame, DP_key_frame_transient, DP_transient_key_frame_decref,
    DP_transient_key_frame_incref, DP_transient_key_frame_new_init,
};
use std::ffi::c_int;

use super::{Attached, CArc, Detached};

pub trait BaseKeyFrame {
    fn persistent_ptr(&self) -> *mut DP_KeyFrame;

    fn transient(&self) -> bool {
        unsafe { DP_key_frame_transient(self.persistent_ptr()) }
    }
}

pub struct TransientKeyFrame {
    data: *mut DP_TransientKeyFrame,
}

pub type AttachedTransientKeyFrame<'a, P> = Attached<'a, TransientKeyFrame, P>;
pub type DetachedTransientKeyFrame = Detached<DP_TransientKeyFrame, TransientKeyFrame>;

impl TransientKeyFrame {
    pub fn new_init(layer_id: c_int, reserve: c_int) -> DetachedTransientKeyFrame {
        let data = unsafe { DP_transient_key_frame_new_init(layer_id, reserve) };
        Detached::new_noinc(TransientKeyFrame { data })
    }
}

impl BaseKeyFrame for TransientKeyFrame {
    fn persistent_ptr(&self) -> *mut DP_KeyFrame {
        self.data.cast()
    }
}

impl CArc<DP_TransientKeyFrame> for TransientKeyFrame {
    unsafe fn incref(&mut self) {
        DP_transient_key_frame_incref(self.data);
    }

    unsafe fn decref(&mut self) {
        DP_transient_key_frame_decref(self.data);
    }

    fn as_mut_ptr(&mut self) -> *mut DP_TransientKeyFrame {
        self.data
    }
}
