// SPDX-License-Identifier: GPL-3.0-or-later
use crate::{
    dp_error, DP_Output, DP_file_output_new_from_path, DP_output_free, DP_output_seek,
    DP_output_tell, DP_output_write,
};
use anyhow::{anyhow, Result};
use std::{ffi::c_char, mem, ptr::null_mut};

pub struct Output {
    output: *mut DP_Output,
}

impl Output {
    pub fn new_from_path(path: *const c_char) -> Result<Output> {
        let output = unsafe { DP_file_output_new_from_path(path) };
        if output.is_null() {
            Err(anyhow!(dp_error()))
        } else {
            Ok(Output { output })
        }
    }

    pub fn leak(self) -> *mut DP_Output {
        let output = self.output;
        mem::forget(self);
        output
    }

    pub fn tell(&self) -> Result<usize> {
        let mut error = false;
        let pos = unsafe { DP_output_tell(self.output, &mut error) };
        if error {
            Err(anyhow!(dp_error()))
        } else {
            Ok(pos)
        }
    }

    pub fn seek(&mut self, offset: usize) -> Result<()> {
        if unsafe { DP_output_seek(self.output, offset) } {
            Ok(())
        } else {
            Err(anyhow!(dp_error()))
        }
    }

    pub fn write_bytes(&mut self, bytes: &[u8]) -> Result<()> {
        if unsafe { DP_output_write(self.output, bytes.as_ptr().cast(), bytes.len()) } {
            Ok(())
        } else {
            Err(anyhow!(dp_error()))
        }
    }

    pub fn write_i16_be(&mut self, value: i16) -> Result<()> {
        self.write_bytes(&value.to_be_bytes())
    }

    pub fn write_u32_be(&mut self, value: u32) -> Result<()> {
        self.write_bytes(&value.to_be_bytes())
    }

    pub fn close(mut self) -> Result<()> {
        if self.free() {
            Ok(())
        } else {
            Err(anyhow!(dp_error()))
        }
    }

    fn free(&mut self) -> bool {
        let output = self.output;
        if output.is_null() {
            true
        } else {
            self.output = null_mut();
            unsafe { DP_output_free(output) }
        }
    }
}

impl Drop for Output {
    fn drop(&mut self) {
        self.free();
    }
}
