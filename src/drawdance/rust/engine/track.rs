// SPDX-License-Identifier: GPL-3.0-or-later
use super::{Attached, CArc, Detached, DetachedTransientKeyFrame};
use crate::{
    DP_Track, DP_TransientTrack, DP_track_transient, DP_transient_track_decref,
    DP_transient_track_id_set, DP_transient_track_incref, DP_transient_track_new_init,
    DP_transient_track_set_transient_noinc, DP_transient_track_title_set,
};
use std::ffi::{c_char, c_int};

pub trait BaseTrack {
    fn persistent_ptr(&self) -> *mut DP_Track;

    fn transient(&self) -> bool {
        unsafe { DP_track_transient(self.persistent_ptr()) }
    }
}

pub struct TransientTrack {
    data: *mut DP_TransientTrack,
}

pub type AttachedTransientTrack<'a, P> = Attached<'a, TransientTrack, P>;
pub type DetachedTransientTrack = Detached<DP_TransientTrack, TransientTrack>;

impl TransientTrack {
    pub fn new_init(reserve: c_int) -> DetachedTransientTrack {
        let data = unsafe { DP_transient_track_new_init(reserve) };
        Detached::new_noinc(Self { data })
    }

    pub fn set_id(&mut self, track_id: c_int) {
        unsafe { DP_transient_track_id_set(self.data, track_id) }
    }

    pub fn set_title(&mut self, title: &[c_char]) {
        unsafe { DP_transient_track_title_set(self.data, title.as_ptr(), title.len()) }
    }

    pub fn set_transient_at_noinc(
        &mut self,
        frame_index: c_int,
        tkf: DetachedTransientKeyFrame,
        index: c_int,
    ) {
        unsafe {
            DP_transient_track_set_transient_noinc(self.data, frame_index, tkf.leak(), index);
        }
    }
}

impl BaseTrack for TransientTrack {
    fn persistent_ptr(&self) -> *mut DP_Track {
        self.data.cast()
    }
}

impl CArc<DP_TransientTrack> for TransientTrack {
    unsafe fn incref(&mut self) {
        DP_transient_track_incref(self.data);
    }

    unsafe fn decref(&mut self) {
        DP_transient_track_decref(self.data);
    }

    fn as_mut_ptr(&mut self) -> *mut DP_TransientTrack {
        self.data
    }
}
