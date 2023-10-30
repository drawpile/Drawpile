// SPDX-License-Identifier: GPL-3.0-or-later
use crate::{
    DP_DocumentMetadata, DP_TransientDocumentMetadata, DP_document_metadata_transient,
    DP_transient_document_metadata_frame_count_set, DP_transient_document_metadata_framerate_set,
};
use std::{ffi::c_int, marker::PhantomData, mem};

// Base interface.

pub trait BaseDocumentMetadata {
    fn persistent_ptr(&self) -> *mut DP_DocumentMetadata;

    fn leak_persistent(self) -> *mut DP_DocumentMetadata
    where
        Self: Sized,
    {
        let data = self.persistent_ptr();
        mem::forget(self);
        data
    }

    fn transient(&self) -> bool {
        unsafe { DP_document_metadata_transient(self.persistent_ptr()) }
    }
}

// Persistent marker.

pub trait BasePersistentDocumentMetadata: BaseDocumentMetadata {}

// Transient interface.

pub trait BaseTransientDocumentMetadata: BaseDocumentMetadata {
    fn transient_ptr(&mut self) -> *mut DP_TransientDocumentMetadata;

    fn leak_transient(mut self) -> *mut DP_TransientDocumentMetadata
    where
        Self: Sized,
    {
        let data = self.transient_ptr();
        mem::forget(self);
        data
    }

    fn set_framerate(&mut self, framerate: c_int) {
        unsafe { DP_transient_document_metadata_framerate_set(self.transient_ptr(), framerate) }
    }

    fn set_frame_count(&mut self, frame_count: c_int) {
        unsafe { DP_transient_document_metadata_frame_count_set(self.transient_ptr(), frame_count) }
    }
}

// Attached persistent type, does not affect refcount.

pub struct AttachedDocumentMetadata<'a, T: ?Sized> {
    data: *mut DP_DocumentMetadata,
    phantom: PhantomData<&'a T>,
}

impl<'a, T: ?Sized> AttachedDocumentMetadata<'a, T> {
    pub(super) fn new(data: *mut DP_DocumentMetadata) -> Self {
        debug_assert!(!data.is_null());
        Self {
            data,
            phantom: PhantomData,
        }
    }
}

impl<'a, T: ?Sized> BaseDocumentMetadata for AttachedDocumentMetadata<'a, T> {
    fn persistent_ptr(&self) -> *mut DP_DocumentMetadata {
        self.data
    }
}

impl<'a, T: ?Sized> BasePersistentDocumentMetadata for AttachedDocumentMetadata<'a, T> {}

// Attached transient type, does not affect refcount.

pub struct AttachedTransientDocumentMetadata<'a, T: ?Sized> {
    data: *mut DP_TransientDocumentMetadata,
    phantom: PhantomData<&'a T>,
}

impl<'a, T: ?Sized> AttachedTransientDocumentMetadata<'a, T> {
    pub(super) fn new(data: *mut DP_TransientDocumentMetadata) -> Self {
        debug_assert!(!data.is_null());
        Self {
            data,
            phantom: PhantomData,
        }
    }
}

impl<'a, T: ?Sized> BaseDocumentMetadata for AttachedTransientDocumentMetadata<'a, T> {
    fn persistent_ptr(&self) -> *mut DP_DocumentMetadata {
        self.data.cast()
    }
}

impl<'a, T: ?Sized> BaseTransientDocumentMetadata for AttachedTransientDocumentMetadata<'a, T> {
    fn transient_ptr(&mut self) -> *mut DP_TransientDocumentMetadata {
        self.data
    }
}
