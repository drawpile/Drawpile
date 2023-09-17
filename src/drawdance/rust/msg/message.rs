// SPDX-License-Identifier: GPL-3.0-or-later
use crate::{
    DP_Message, DP_MessageType, DP_message_decref_nullable, DP_message_length, DP_message_type,
    DP_message_type_name,
};
use std::{ffi::CStr, ptr};

#[repr(C)]
pub struct Message {
    msg: *mut DP_Message,
}

impl Message {
    pub fn message_type_name(message_type: DP_MessageType) -> String {
        unsafe { CStr::from_ptr(DP_message_type_name(message_type)) }
            .to_str()
            .unwrap()
            .to_owned()
    }

    pub fn new_noinc(msg: *mut DP_Message) -> Self {
        Message { msg }
    }

    pub fn message_type(&self) -> DP_MessageType {
        unsafe { DP_message_type(self.msg) }
    }

    pub fn length(&self) -> usize {
        unsafe { DP_message_length(self.msg) }
    }

    pub fn move_to_ptr(mut self) -> *mut DP_Message {
        let msg = self.msg;
        self.msg = ptr::null_mut();
        msg
    }
}

impl Drop for Message {
    fn drop(&mut self) {
        unsafe { DP_message_decref_nullable(self.msg) }
    }
}
