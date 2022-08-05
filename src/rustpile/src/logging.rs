use tracing::{debug, Level, Metadata};
use tracing_subscriber::fmt::{MakeWriter, Subscriber};

use std::{ffi::CString, io::Result, io::Write, os::raw::c_char, ptr};

pub type ExtLogFn =
    extern "C" fn(level: i32, file: *const c_char, line: u32, logmsg: *const c_char);

struct ExtLogWriter {
    log_fn: ExtLogFn,
    file: Option<CString>,
    line: Option<u32>,
    level: i32,
}

impl Write for ExtLogWriter {
    fn write(&mut self, buf: &[u8]) -> Result<usize> {
        // Strip newline character since Qt's logger will
        // insert it itself.
        let msg = buf.strip_suffix(&[b'\n']).unwrap_or(buf);

        let cstr = match CString::new(msg) {
            Ok(s) => s,
            Err(nul) => {
                let np = nul.nul_position();
                let mut v = nul.into_vec();
                v.truncate(np);
                //CString::from_vec_with_nul (unstable API)
                CString::new(v).unwrap()
            }
        };

        (self.log_fn)(
            self.level,
            self.file
                .as_ref()
                .map(|s| s.as_ptr())
                .unwrap_or(ptr::null()),
            self.line.unwrap_or(0),
            cstr.as_ptr(),
        );
        Ok(buf.len())
    }

    fn flush(&mut self) -> Result<()> {
        Ok(())
    }
}

struct ExtLogger(ExtLogFn);

impl MakeWriter for ExtLogger {
    type Writer = ExtLogWriter;
    fn make_writer(&self) -> Self::Writer {
        ExtLogWriter {
            log_fn: self.0,
            file: None,
            line: None,
            level: 0,
        }
    }

    fn make_writer_for(&self, meta: &Metadata<'_>) -> Self::Writer {
        ExtLogWriter {
            log_fn: self.0,
            file: meta.file().and_then(|s| CString::new(s).ok()),
            line: meta.line(),
            level: match *meta.level() {
                Level::INFO => 1,
                Level::WARN => 2,
                Level::ERROR => 3,
                _ => 0,
            },
        }
    }
}

#[no_mangle]
pub extern "C" fn rustpile_init_logging(log_writer: ExtLogFn) {
    Subscriber::builder()
        .with_max_level(Level::DEBUG)
        .with_level(false)
        .without_time()
        .with_writer(ExtLogger(log_writer))
        .init();

    debug!("Initialized logging.");
}
