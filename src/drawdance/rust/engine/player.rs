// SPDX-License-Identifier: GPL-3.0-or-later
use crate::{
    dp_error_anyhow, json_object_get_string, json_value_get_object, msg::Message, DP_Input,
    DP_Message, DP_Player, DP_PlayerCompatibility, DP_PlayerPass, DP_PlayerType,
    DP_file_input_new_from_path, DP_file_input_new_from_stdin, DP_player_acl_override_set,
    DP_player_compatibility, DP_player_compatible, DP_player_free, DP_player_header, DP_player_new,
    DP_player_pass_set, DP_player_step, DP_player_type, JSON_Value, DP_PLAYER_RECORDING_END,
    DP_PLAYER_SUCCESS,
};
use anyhow::{anyhow, Result};
use std::{
    ffi::{c_char, CStr, CString},
    ptr::{self, null_mut},
};

pub struct Player {
    player: *mut DP_Player,
}

impl Player {
    pub fn new_from_stdin(ptype: DP_PlayerType) -> Result<Self> {
        let input = unsafe { DP_file_input_new_from_stdin(true) };
        if input.is_null() {
            return Err(dp_error_anyhow());
        }
        Self::new(ptype, ptr::null(), input)
    }

    pub fn new_from_path(ptype: DP_PlayerType, path: String) -> Result<Self> {
        let cpath = CString::new(path)?;
        let input = unsafe { DP_file_input_new_from_path(cpath.as_ptr()) };
        if input.is_null() {
            return Err(dp_error_anyhow());
        }
        Self::new(ptype, cpath.as_ptr(), input)
    }

    fn new(
        ptype: DP_PlayerType,
        path_or_null: *const c_char,
        input: *mut DP_Input,
    ) -> Result<Self> {
        let player = unsafe { DP_player_new(ptype, path_or_null, input, null_mut()) };
        if player.is_null() {
            Err(dp_error_anyhow())
        } else {
            Ok(Player { player })
        }
    }

    pub fn move_to_ptr(mut self) -> *mut DP_Player {
        let player = self.player;
        self.player = ptr::null_mut();
        player
    }

    pub fn player_type(&self) -> DP_PlayerType {
        unsafe { DP_player_type(self.player) }
    }

    pub fn header(&self) -> *mut JSON_Value {
        unsafe { DP_player_header(self.player) }
    }

    pub fn format_version(&self) -> Option<String> {
        self.get_string_from_header("version")
    }

    pub fn writer_version(&self) -> Option<String> {
        self.get_string_from_header("writerversion")
    }

    fn get_string_from_header(&self, key: &str) -> Option<String> {
        let ckey = match CString::new(key) {
            Ok(s) => s,
            Err(_) => return None,
        };

        let header = self.header();
        if header.is_null() {
            return None;
        }

        let value = unsafe { json_object_get_string(json_value_get_object(header), ckey.as_ptr()) };
        if value.is_null() {
            return None;
        }

        Some(
            unsafe { CStr::from_ptr(value) }
                .to_str()
                .unwrap()
                .to_owned(),
        )
    }

    pub fn compatibility(&self) -> DP_PlayerCompatibility {
        unsafe { DP_player_compatibility(self.player) }
    }

    pub fn is_compatible(&self) -> bool {
        unsafe { DP_player_compatible(self.player) }
    }

    pub fn check_compatible(self) -> Result<Player> {
        if self.is_compatible() {
            Ok(self)
        } else {
            Err(anyhow!("Incompatible recording"))
        }
    }

    pub fn set_acl_override(&mut self, acl_override: bool) {
        unsafe { DP_player_acl_override_set(self.player, acl_override) }
    }

    pub fn set_pass(&mut self, pass: DP_PlayerPass) {
        unsafe { DP_player_pass_set(self.player, pass) }
    }

    pub fn step(&mut self) -> Result<Option<Message>> {
        let mut msg: *mut DP_Message = ptr::null_mut();
        let result = unsafe { DP_player_step(self.player, &mut msg) };
        if result == DP_PLAYER_SUCCESS {
            Ok(Some(Message::new_noinc(msg)))
        } else if result == DP_PLAYER_RECORDING_END {
            Ok(None)
        } else {
            Err(dp_error_anyhow())
        }
    }
}

impl Drop for Player {
    fn drop(&mut self) {
        unsafe { DP_player_free(self.player) }
    }
}
