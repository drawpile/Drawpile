// SPDX-License-Identifier: GPL-3.0-or-later
use std::{
    marker::PhantomData,
    mem,
    ops::{Deref, DerefMut},
};

// C-side atomic reference counted types.

pub trait CArc<T> {
    unsafe fn incref(&mut self);
    unsafe fn decref(&mut self);
    fn as_mut_ptr(&mut self) -> *mut T;
}

// Transient type that can be persisted.

pub trait Persistable<P, C: CArc<P>> {
    unsafe fn persist(&mut self) -> Detached<P, C>;
}

// Goofy extra trait because we can't have unbounded generics.

pub trait Persister<P, C: CArc<P>> {
    fn persist(self) -> Detached<P, C>;
}

// Type attached to a parent, doesn't affect refcount.

pub struct Attached<'a, T, P> {
    content: T,
    phantom: PhantomData<&'a P>,
}

impl<'a, T, P> Attached<'a, T, P> {
    pub fn new(content: T) -> Self {
        Self {
            content,
            phantom: PhantomData,
        }
    }
}

impl<'a, T, P> Deref for Attached<'a, T, P> {
    type Target = T;

    fn deref(&self) -> &Self::Target {
        &self.content
    }
}

impl<'a, T, P> DerefMut for Attached<'a, T, P> {
    fn deref_mut(&mut self) -> &mut Self::Target {
        &mut self.content
    }
}

// Detached type, affects refcount.

pub struct Detached<P, C: CArc<P>> {
    content: C,
    phantom: PhantomData<P>,
}

impl<P, C: CArc<P>> Detached<P, C> {
    pub fn new_inc(mut content: C) -> Self {
        unsafe {
            content.incref();
        }
        Self::new_noinc(content)
    }

    pub fn new_noinc(content: C) -> Self {
        Self {
            content,
            phantom: PhantomData,
        }
    }

    pub fn leak(mut self) -> *mut P {
        let data = self.content.as_mut_ptr();
        mem::forget(self);
        data
    }
}

// Rust doesn't allow unbounded generics here, so this is totally unreadable.
// The function just eats the transient version and return a persistent one.
impl<Q, D: CArc<Q>, P, C: CArc<P> + Persistable<Q, D>> Persister<Q, D> for Detached<P, C> {
    fn persist(mut self) -> Detached<Q, D> {
        let persisted = unsafe { self.content.persist() };
        mem::forget(self);
        persisted
    }
}

impl<P, C: CArc<P>> Deref for Detached<P, C> {
    type Target = C;

    fn deref(&self) -> &Self::Target {
        &self.content
    }
}

impl<P, C: CArc<P>> DerefMut for Detached<P, C> {
    fn deref_mut(&mut self) -> &mut Self::Target {
        &mut self.content
    }
}

impl<P, C: CArc<P>> Drop for Detached<P, C> {
    fn drop(&mut self) {
        unsafe {
            self.decref();
        }
    }
}
