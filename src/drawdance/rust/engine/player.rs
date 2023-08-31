// SPDX-License-Identifier: GPL-3.0-or-later
use crate::{
    dp_error, json_object_get_string, json_value_get_object, msg::Message, DP_Input, DP_LoadResult,
    DP_Message, DP_Player, DP_PlayerResult, DP_PlayerType, DP_file_input_new_from_path,
    DP_file_input_new_from_stdin, DP_player_acl_override_set, DP_player_compatible, DP_player_free,
    DP_player_header, DP_player_new, DP_player_step, DP_player_type, JSON_Value,
    DP_LOAD_RESULT_SUCCESS, DP_PLAYER_RECORDING_END, DP_PLAYER_SUCCESS,
};
use std::{
    ffi::{c_char, CStr, CString, NulError},
    ptr,
};

pub struct Player {
    player: *mut DP_Player,
}

#[derive(Debug)]
pub enum PlayerError {
    DpError(String),
    LoadError(DP_LoadResult, String),
    ResultError(DP_PlayerResult, String),
    NulError(NulError),
}

impl PlayerError {
    fn from_dp_error() -> Self {
        Self::DpError(dp_error())
    }

    fn from_load_result(result: DP_LoadResult) -> Self {
        if result == DP_LOAD_RESULT_SUCCESS {
            Self::from_dp_error()
        } else {
            Self::LoadError(result, dp_error())
        }
    }

    fn from_player_result(result: DP_PlayerResult) -> Self {
        if result == DP_PLAYER_SUCCESS {
            Self::from_dp_error()
        } else {
            Self::ResultError(result, dp_error())
        }
    }
}

impl From<NulError> for PlayerError {
    fn from(err: NulError) -> Self {
        Self::NulError(err)
    }
}

impl Player {
    pub fn new_from_stdin(ptype: DP_PlayerType) -> Result<Self, PlayerError> {
        let input = unsafe { DP_file_input_new_from_stdin(true) };
        if input.is_null() {
            return Err(PlayerError::from_dp_error());
        }
        Self::new(ptype, ptr::null(), input)
    }

    pub fn new_from_path(ptype: DP_PlayerType, path: String) -> Result<Self, PlayerError> {
        let cpath = CString::new(path)?;
        let input = unsafe { DP_file_input_new_from_path(cpath.as_ptr()) };
        if input.is_null() {
            return Err(PlayerError::from_dp_error());
        }
        Self::new(ptype, cpath.as_ptr(), input)
    }

    fn new(
        ptype: DP_PlayerType,
        path_or_null: *const c_char,
        input: *mut DP_Input,
    ) -> Result<Self, PlayerError> {
        let mut result: DP_LoadResult = 0;
        let player = unsafe { DP_player_new(ptype, path_or_null, input, &mut result) };
        if player.is_null() {
            Err(PlayerError::from_load_result(result))
        } else {
            Ok(Player { player })
        }
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

    pub fn is_compatible(&self) -> bool {
        unsafe { DP_player_compatible(self.player) }
    }

    pub fn set_acl_override(&mut self, acl_override: bool) {
        unsafe { DP_player_acl_override_set(self.player, acl_override) }
    }

    pub fn step(&mut self) -> Result<Option<Message>, PlayerError> {
        let mut msg: *mut DP_Message = ptr::null_mut();
        let result = unsafe { DP_player_step(self.player, &mut msg) };
        if result == DP_PLAYER_SUCCESS {
            Ok(Some(Message::new_noinc(msg)))
        } else if result == DP_PLAYER_RECORDING_END {
            Ok(None)
        } else {
            Err(PlayerError::from_player_result(result))
        }
    }
}

impl Drop for Player {
    fn drop(&mut self) {
        unsafe { DP_player_free(self.player) }
    }
}
