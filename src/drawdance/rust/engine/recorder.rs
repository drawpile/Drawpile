// SPDX-License-Identifier: GPL-3.0-or-later
use crate::{
    dp_error_anyhow, msg::Message, DP_Output, DP_Recorder, DP_RecorderType,
    DP_file_output_new_from_path, DP_file_output_new_from_stdout, DP_free, DP_recorder_free_join,
    DP_recorder_header_clone, DP_recorder_message_push_noinc, DP_recorder_new_inc, JSON_Value,
};
use anyhow::{anyhow, Result};
use std::{
    ffi::{c_char, CStr, CString},
    ptr,
};

pub struct Recorder {
    recorder: *mut DP_Recorder,
}

impl Recorder {
    pub fn new_from_stdout(rtype: DP_RecorderType, header: *mut JSON_Value) -> Result<Self> {
        let output = unsafe { DP_file_output_new_from_stdout(true) };
        if output.is_null() {
            return Err(dp_error_anyhow());
        }
        Self::new(rtype, header, output)
    }

    pub fn new_from_path(
        rtype: DP_RecorderType,
        header: *mut JSON_Value,
        path: String,
    ) -> Result<Self> {
        let cpath = CString::new(path)?;
        let output = unsafe { DP_file_output_new_from_path(cpath.as_ptr()) };
        if output.is_null() {
            return Err(dp_error_anyhow());
        }
        Self::new(rtype, header, output)
    }

    fn new(
        rtype: DP_RecorderType,
        header: *mut JSON_Value,
        output: *mut DP_Output,
    ) -> Result<Self> {
        let cloned_header = unsafe { DP_recorder_header_clone(header) };
        if cloned_header.is_null() {
            return Err(dp_error_anyhow());
        }

        let recorder = unsafe {
            DP_recorder_new_inc(
                rtype,
                cloned_header,
                ptr::null_mut(),
                None,
                ptr::null_mut(),
                output,
            )
        };
        if recorder.is_null() {
            Err(dp_error_anyhow())
        } else {
            Ok(Recorder { recorder })
        }
    }

    pub fn push_noinc(&mut self, msg: Message) -> bool {
        unsafe { DP_recorder_message_push_noinc(self.recorder, msg.move_to_ptr()) }
    }

    pub fn dispose(mut self) -> Result<()> {
        let mut error: *mut c_char = ptr::null_mut();
        unsafe { DP_recorder_free_join(self.recorder, &mut error) };
        self.recorder = ptr::null_mut();
        if error.is_null() {
            Ok(())
        } else {
            let message = unsafe { CStr::from_ptr(error) }
                .to_str()
                .unwrap()
                .to_owned();
            unsafe { DP_free(error.cast()) }
            Err(anyhow!(message))
        }
    }
}

impl Drop for Recorder {
    fn drop(&mut self) {
        unsafe { DP_recorder_free_join(self.recorder, ptr::null_mut()) }
    }
}
