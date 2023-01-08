use dpcore::paint::LayerStack;
use dpcore::protocol::message::Message;
use dpimpex::rec::reader::{open_recording, Compatibility, ReadMessage};
use dpimpex::rec_index::reader::{read_index, read_snapshot, read_thumbnail, Index};
use dpimpex::rec_index::writer::IndexBuilder;
use dpimpex::rec_index::{IndexEntry, IndexError, IndexResult};

use core::ffi::c_void;
use std::fs::File;
use std::path::Path;
use std::time::{Duration, Instant};
use tracing::warn;

pub struct IndexReader {
    reader: File,
    read_index: Index,
    thumbnails: Vec<u64>,
    last_read_thumbnail: Vec<u8>,
}

impl IndexReader {
    pub fn open(path: &Path) -> IndexResult<IndexReader> {
        let mut reader = File::open(path)?;

        let read_index = read_index(&mut reader)?;

        let thumbnails = read_index
            .entries
            .iter()
            .filter(|e| e.thumbnail_offset > 0)
            .map(|e| e.thumbnail_offset)
            .collect();

        Ok(IndexReader {
            reader,
            read_index,
            thumbnails,
            last_read_thumbnail: Vec::new(),
        })
    }

    pub fn message_count(&self) -> u32 {
        self.read_index.messages
    }

    pub fn thumbnail_count(&self) -> u32 {
        self.thumbnails.len() as u32
    }

    pub fn read_thumbnail(&mut self, index: usize) -> IndexResult<&[u8]> {
        let thumbnail = read_thumbnail(&mut self.reader, self.thumbnails[index])?;
        self.last_read_thumbnail = thumbnail;
        Ok(&self.last_read_thumbnail)
    }

    /// Load the snapshot closest to the given message index
    pub fn load_snapshot(&mut self, msg_index: u32) -> IndexResult<(u32, u64, LayerStack)> {
        let mut entry: Option<&IndexEntry> = None;

        for i in self.read_index.entries.iter().rev() {
            if i.message_index <= msg_index {
                entry = Some(i);
                break;
            }
        }

        if let Some(entry) = entry {
            Ok((
                entry.message_index,
                entry.message_offset,
                read_snapshot(&mut self.reader, entry)?,
            ))
        } else {
            Ok((0, 0, LayerStack::new(0, 0)))
        }
    }
}

pub type IndexBuildProgressNoticationFn = extern "C" fn(ctx: *mut c_void, progress: u32);

pub fn build_index(
    input_path: &Path,
    ctx: *mut c_void,
    progress_notification_func: IndexBuildProgressNoticationFn,
) -> IndexResult<()> {
    let mut reader = open_recording(input_path)?;

    if reader.check_compatibility() == Compatibility::Incompatible {
        return Err(IndexError::IncompatibleVersion);
    }

    let mut indexer = IndexBuilder::new(File::create(input_path.with_extension("dpidx"))?);

    indexer.write_header()?;
    let mut last_snapshot = Instant::now();
    let mut thumbnail_counter: i32 = 0;
    let mut last_progress: u32 = 0;

    loop {
        let offset = reader.current_offset();
        match reader.read_next() {
            ReadMessage::Ok(Message::Command(m)) => {
                indexer.add_message(&m, offset);

                thumbnail_counter -= 1;
                if last_snapshot.elapsed() > Duration::from_millis(100) {
                    indexer.make_entry(thumbnail_counter <= 0)?;

                    if thumbnail_counter <= 0 {
                        thumbnail_counter = 1000;
                    }

                    last_snapshot = Instant::now();
                }

                let progress = (reader.current_progress() * 100.0) as u32;
                if progress > last_progress {
                    (progress_notification_func)(ctx, progress);
                    last_progress = progress;
                }
            }
            ReadMessage::Ok(_) => (),
            ReadMessage::Invalid(msg) => {
                warn!("Invalid message: {}", msg);
            }
            ReadMessage::IoError(e) => {
                return Err(e.into());
            }
            ReadMessage::Eof => {
                break;
            }
        }
    }

    // Always include a thumbnail at the very end of the file
    indexer.make_entry(true)?;

    indexer.finalize()?;
    Ok(())
}
