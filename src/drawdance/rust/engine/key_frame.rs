// SPDX-License-Identifier: GPL-3.0-or-later
use crate::{
    DP_KeyFrame, DP_TransientKeyFrame, DP_key_frame_transient, DP_transient_key_frame_decref,
    DP_transient_key_frame_new_init,
};
use std::{ffi::c_int, mem};

// Base interface.

pub trait BaseKeyFrame {
    fn persistent_ptr(&self) -> *mut DP_KeyFrame;

    fn leak_persistent(self) -> *mut DP_KeyFrame
    where
        Self: Sized,
    {
        let data = self.persistent_ptr();
        mem::forget(self);
        data
    }

    fn transient(&self) -> bool {
        unsafe { DP_key_frame_transient(self.persistent_ptr()) }
    }
}

// Persistent marker.

pub trait BasePersistentKeyFrame: BaseKeyFrame {}

// Transient interface.

pub trait BaseTransientKeyFrame: BaseKeyFrame {
    fn transient_ptr(&mut self) -> *mut DP_TransientKeyFrame;

    fn leak_transient(mut self) -> *mut DP_TransientKeyFrame
    where
        Self: Sized,
    {
        let data = self.transient_ptr();
        mem::forget(self);
        data
    }
}

// Free transient type, affects refcount.

pub struct TransientKeyFrame {
    data: *mut DP_TransientKeyFrame,
}

impl TransientKeyFrame {
    pub fn new_init(layer_id: c_int, reserve: c_int) -> Self {
        let data = unsafe { DP_transient_key_frame_new_init(layer_id, reserve) };
        TransientKeyFrame { data }
    }
}

impl BaseKeyFrame for TransientKeyFrame {
    fn persistent_ptr(&self) -> *mut DP_KeyFrame {
        self.data.cast()
    }
}

impl BaseTransientKeyFrame for TransientKeyFrame {
    fn transient_ptr(&mut self) -> *mut DP_TransientKeyFrame {
        self.data
    }
}

impl Drop for TransientKeyFrame {
    fn drop(&mut self) {
        unsafe { DP_transient_key_frame_decref(self.data) }
    }
}
