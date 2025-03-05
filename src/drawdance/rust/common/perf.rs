// SPDX-License-Identifier: GPL-3.0-or-later
use super::Output;
use crate::{dp_error_anyhow, DP_perf_close, DP_perf_is_open, DP_perf_open};
use anyhow::Result;
use std::{env, ffi::CString};

pub struct Perf;

impl Perf {
    pub fn new_from_env() -> Result<Perf> {
        let path = env::var("DRAWPILE_PERF_PATH")?;
        let cpath = CString::new(path)?;
        let output = Output::new_from_path(cpath.as_ptr())?;
        if unsafe { DP_perf_open(output.leak()) } {
            Ok(Perf {})
        } else {
            Err(dp_error_anyhow())
        }
    }
}

impl Drop for Perf {
    fn drop(&mut self) {
        if unsafe { DP_perf_is_open() } {
            unsafe {
                DP_perf_close();
            }
        }
    }
}
