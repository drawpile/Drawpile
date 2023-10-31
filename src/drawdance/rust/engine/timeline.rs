// SPDX-License-Identifier: GPL-3.0-or-later
use super::{Attached, CArc, Detached, DetachedTransientTrack};
use crate::{
    DP_Timeline, DP_TransientTimeline, DP_timeline_transient, DP_transient_timeline_decref,
    DP_transient_timeline_incref, DP_transient_timeline_new_init,
    DP_transient_timeline_set_transient_noinc,
};
use std::ffi::c_int;

pub trait BaseTimeline {
    fn persistent_ptr(&self) -> *mut DP_Timeline;

    fn transient(&self) -> bool {
        unsafe { DP_timeline_transient(self.persistent_ptr()) }
    }
}

pub struct TransientTimeline {
    data: *mut DP_TransientTimeline,
}

pub type AttachedTransientTimeline<'a, P> = Attached<'a, TransientTimeline, P>;
pub type DetachedTransientTimeline = Detached<DP_TransientTimeline, TransientTimeline>;

impl TransientTimeline {
    pub fn new_init(reserve: c_int) -> DetachedTransientTimeline {
        let data = unsafe { DP_transient_timeline_new_init(reserve) };
        Detached::new_noinc(Self { data })
    }

    pub fn set_transient_at_noinc(&mut self, tt: DetachedTransientTrack, index: c_int) {
        unsafe {
            DP_transient_timeline_set_transient_noinc(self.data, tt.leak(), index);
        }
    }
}

impl BaseTimeline for TransientTimeline {
    fn persistent_ptr(&self) -> *mut DP_Timeline {
        self.data.cast()
    }
}

impl CArc<DP_TransientTimeline> for TransientTimeline {
    unsafe fn incref(&mut self) {
        DP_transient_timeline_incref(self.data);
    }

    unsafe fn decref(&mut self) {
        DP_transient_timeline_decref(self.data);
    }

    fn as_mut_ptr(&mut self) -> *mut DP_TransientTimeline {
        self.data
    }
}
