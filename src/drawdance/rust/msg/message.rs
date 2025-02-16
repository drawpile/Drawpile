// SPDX-License-Identifier: GPL-3.0-or-later
use crate::{
    DP_Message, DP_MessageType, DP_message_context_id, DP_message_decref_nullable,
    DP_message_internal, DP_message_length, DP_message_type, DP_message_type_command,
    DP_message_type_name, DP_msg_join_name, DP_msg_undo_redo, DP_MSG_JOIN, DP_MSG_PEN_UP,
    DP_MSG_UNDO,
};
use std::{
    ffi::{c_uint, CStr},
    ptr,
    str::from_utf8,
};

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

    pub fn is_command(&self) -> bool {
        unsafe { DP_message_type_command(self.message_type()) }
    }

    pub fn length(&self) -> usize {
        unsafe { DP_message_length(self.msg) }
    }

    pub fn context_id(&self) -> c_uint {
        unsafe { DP_message_context_id(self.msg) }
    }

    pub fn move_to_ptr(mut self) -> *mut DP_Message {
        let msg = self.msg;
        self.msg = ptr::null_mut();
        msg
    }

    pub fn to_internal(&self) -> InternalMessage {
        match self.message_type() {
            DP_MSG_JOIN => InternalMessage::Join(MsgJoin { parent: self }),
            DP_MSG_PEN_UP => InternalMessage::PenUp(),
            DP_MSG_UNDO => InternalMessage::Undo(MsgUndo { parent: self }),
            _ => InternalMessage::Unknown(),
        }
    }
}

impl Drop for Message {
    fn drop(&mut self) {
        unsafe { DP_message_decref_nullable(self.msg) }
    }
}

pub struct MsgJoin<'a> {
    parent: &'a Message,
}

impl<'a> MsgJoin<'a> {
    pub fn name(&self) -> String {
        unsafe {
            let mut len: usize = 0;
            let name: *const u8 =
                DP_msg_join_name(DP_message_internal(self.parent.msg).cast(), &mut len).cast();
            from_utf8(ptr::slice_from_raw_parts(name, len).as_ref().unwrap())
        }
        .map_or_else(|_| String::default(), ToOwned::to_owned)
    }
}

pub struct MsgUndo<'a> {
    parent: &'a Message,
}

impl<'a> MsgUndo<'a> {
    pub fn is_redo(&self) -> bool {
        unsafe { DP_msg_undo_redo(DP_message_internal(self.parent.msg).cast()) }
    }
}

pub enum InternalMessage<'a> {
    Unknown(),
    Join(MsgJoin<'a>),
    PenUp(),
    Undo(MsgUndo<'a>),
}
