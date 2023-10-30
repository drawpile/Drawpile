// SPDX-License-Identifier: GPL-3.0-or-later
use super::{BaseTransientTrack, TransientTrack};
use crate::{
    DP_Timeline, DP_TransientTimeline, DP_timeline_transient, DP_transient_timeline_decref,
    DP_transient_timeline_new_init, DP_transient_timeline_set_transient_noinc,
};
use std::{ffi::c_int, mem};

// Base interface.

pub trait BaseTimeline {
    fn persistent_ptr(&self) -> *mut DP_Timeline;

    fn leak_persistent(self) -> *mut DP_Timeline
    where
        Self: Sized,
    {
        let data = self.persistent_ptr();
        mem::forget(self);
        data
    }

    fn transient(&self) -> bool {
        unsafe { DP_timeline_transient(self.persistent_ptr()) }
    }
}

// Persistent marker.

pub trait BasePersistentTimeline: BaseTimeline {}

// Transient interface.

pub trait BaseTransientTimeline: BaseTimeline {
    fn transient_ptr(&mut self) -> *mut DP_TransientTimeline;

    fn leak_transient(mut self) -> *mut DP_TransientTimeline
    where
        Self: Sized,
    {
        let data = self.transient_ptr();
        mem::forget(self);
        data
    }

    fn set_transient_at_noinc(&mut self, tt: TransientTrack, index: c_int) {
        unsafe {
            DP_transient_timeline_set_transient_noinc(
                self.transient_ptr(),
                tt.leak_transient(),
                index,
            );
        }
    }
}

// Free transient type, affects refcount.

pub struct TransientTimeline {
    data: *mut DP_TransientTimeline,
}

impl TransientTimeline {
    pub fn new(reserve: c_int) -> Self {
        let data = unsafe { DP_transient_timeline_new_init(reserve) };
        TransientTimeline { data }
    }
}

impl BaseTimeline for TransientTimeline {
    fn persistent_ptr(&self) -> *mut DP_Timeline {
        self.data.cast()
    }
}

impl BaseTransientTimeline for TransientTimeline {
    fn transient_ptr(&mut self) -> *mut DP_TransientTimeline {
        self.data
    }
}

impl Drop for TransientTimeline {
    fn drop(&mut self) {
        unsafe { DP_transient_timeline_decref(self.data) }
    }
}
