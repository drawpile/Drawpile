// SPDX-License-Identifier: GPL-3.0-or-later
use super::{BaseTransientKeyFrame, TransientKeyFrame};
use crate::{
    DP_Track, DP_TransientTrack, DP_track_transient, DP_transient_track_decref,
    DP_transient_track_id_set, DP_transient_track_new_init, DP_transient_track_set_transient_noinc,
    DP_transient_track_title_set,
};
use std::{
    ffi::{c_char, c_int},
    mem,
};

// Base interface.

pub trait BaseTrack {
    fn persistent_ptr(&self) -> *mut DP_Track;

    fn leak_persistent(self) -> *mut DP_Track
    where
        Self: Sized,
    {
        let data = self.persistent_ptr();
        mem::forget(self);
        data
    }

    fn transient(&self) -> bool {
        unsafe { DP_track_transient(self.persistent_ptr()) }
    }
}

// Persistent marker.

pub trait BasePersistentTrack: BaseTrack {}

// Transient interface.

pub trait BaseTransientTrack: BaseTrack {
    fn transient_ptr(&mut self) -> *mut DP_TransientTrack;

    fn leak_transient(mut self) -> *mut DP_TransientTrack
    where
        Self: Sized,
    {
        let data = self.transient_ptr();
        mem::forget(self);
        data
    }

    fn set_id(&mut self, track_id: c_int) {
        unsafe { DP_transient_track_id_set(self.transient_ptr(), track_id) }
    }

    fn set_title(&mut self, title: &[c_char]) {
        unsafe { DP_transient_track_title_set(self.transient_ptr(), title.as_ptr(), title.len()) }
    }

    fn set_transient_at_noinc(&mut self, frame_index: c_int, tkf: TransientKeyFrame, index: c_int) {
        unsafe {
            DP_transient_track_set_transient_noinc(
                self.transient_ptr(),
                frame_index,
                tkf.leak_transient(),
                index,
            )
        }
    }
}

// Free transient type, affects refcount.

pub struct TransientTrack {
    data: *mut DP_TransientTrack,
}

impl TransientTrack {
    pub fn new_init(reserve: c_int) -> Self {
        let data = unsafe { DP_transient_track_new_init(reserve) };
        TransientTrack { data }
    }
}

impl BaseTrack for TransientTrack {
    fn persistent_ptr(&self) -> *mut DP_Track {
        self.data.cast()
    }
}

impl BaseTransientTrack for TransientTrack {
    fn transient_ptr(&mut self) -> *mut DP_TransientTrack {
        self.data
    }
}

impl Drop for TransientTrack {
    fn drop(&mut self) {
        unsafe { DP_transient_track_decref(self.data) }
    }
}
