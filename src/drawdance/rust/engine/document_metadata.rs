// SPDX-License-Identifier: GPL-3.0-or-later
use super::{Attached, CArc, Detached};
use crate::{
    DP_DocumentMetadata, DP_TransientDocumentMetadata, DP_document_metadata_decref,
    DP_document_metadata_incref, DP_document_metadata_transient,
    DP_transient_document_metadata_decref, DP_transient_document_metadata_frame_count_set,
    DP_transient_document_metadata_framerate_set, DP_transient_document_metadata_incref,
};
use std::ffi::c_int;

pub trait BaseDocumentMetadata {
    fn persistent_ptr(&self) -> *mut DP_DocumentMetadata;

    fn transient(&self) -> bool {
        unsafe { DP_document_metadata_transient(self.persistent_ptr()) }
    }
}

pub struct DocumentMetadata {
    data: *mut DP_DocumentMetadata,
}

pub type AttachedDocumentMetadata<'a, P> = Attached<'a, DocumentMetadata, P>;
pub type DetachedDocumentMetadata = Detached<DP_DocumentMetadata, DocumentMetadata>;

impl DocumentMetadata {
    pub fn new_attached<P>(data: &mut DP_DocumentMetadata) -> AttachedDocumentMetadata<P> {
        Attached::new(Self { data })
    }
}

impl BaseDocumentMetadata for DocumentMetadata {
    fn persistent_ptr(&self) -> *mut DP_DocumentMetadata {
        self.data
    }
}

impl CArc<DP_DocumentMetadata> for DocumentMetadata {
    unsafe fn incref(&mut self) {
        DP_document_metadata_incref(self.data);
    }

    unsafe fn decref(&mut self) {
        DP_document_metadata_decref(self.data);
    }

    fn as_mut_ptr(&mut self) -> *mut DP_DocumentMetadata {
        self.data
    }
}

pub struct TransientDocumentMetadata {
    data: *mut DP_TransientDocumentMetadata,
}

pub type AttachedTransientDocumentMetadata<'a, P> = Attached<'a, TransientDocumentMetadata, P>;
pub type DetachedTransientDocumentMetadata =
    Detached<DP_TransientDocumentMetadata, TransientDocumentMetadata>;

impl TransientDocumentMetadata {
    pub fn new_attached<P>(data: &mut DP_DocumentMetadata) -> AttachedTransientDocumentMetadata<P> {
        Attached::new(Self { data })
    }

    pub fn set_framerate(&mut self, framerate: c_int) {
        unsafe { DP_transient_document_metadata_framerate_set(self.data, framerate) }
    }

    pub fn set_frame_count(&mut self, frame_count: c_int) {
        unsafe { DP_transient_document_metadata_frame_count_set(self.data, frame_count) }
    }
}

impl BaseDocumentMetadata for TransientDocumentMetadata {
    fn persistent_ptr(&self) -> *mut DP_TransientDocumentMetadata {
        self.data
    }
}

impl CArc<DP_TransientDocumentMetadata> for TransientDocumentMetadata {
    unsafe fn incref(&mut self) {
        DP_transient_document_metadata_incref(self.data);
    }

    unsafe fn decref(&mut self) {
        DP_transient_document_metadata_decref(self.data);
    }

    fn as_mut_ptr(&mut self) -> *mut DP_TransientDocumentMetadata {
        self.data
    }
}
