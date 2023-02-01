use std::{error, fmt};

pub mod reader;
pub mod writer;

pub const INDEX_FORMAT_VERSION: u16 = 10;

pub struct IndexEntry {
    /// Number of the message this entry points to
    pub message_index: u32,

    /// Offset of the message in the recording file
    pub message_offset: u64,

    /// Offset of the snapshot in the index file
    pub snapshot_offset: u64,

    /// Offset of the thumbnail in the index file (if nonzero)
    pub thumbnail_offset: u64,
}

#[derive(Debug)]
pub enum IndexError {
    IoError(std::io::Error),
    ConversionError(std::num::TryFromIntError),
    IncompatibleVersion,
    BadTile,
    CorruptFile,
    BadString(std::string::FromUtf8Error),
    BadImage(image::ImageError),
}

impl From<std::io::Error> for IndexError {
    fn from(e: std::io::Error) -> Self {
        Self::IoError(e)
    }
}

impl From<std::num::TryFromIntError> for IndexError {
    fn from(e: std::num::TryFromIntError) -> Self {
        Self::ConversionError(e)
    }
}

impl From<std::string::FromUtf8Error> for IndexError {
    fn from(e: std::string::FromUtf8Error) -> Self {
        Self::BadString(e)
    }
}

impl From<image::ImageError> for IndexError {
    fn from(e: image::ImageError) -> Self {
        Self::BadImage(e)
    }
}

impl fmt::Display for IndexError {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match self {
            IndexError::IoError(e) => write!(f, "IO error: {e}"),
            IndexError::ConversionError(e) => write!(f, "Number too big! ({e})"),
            IndexError::IncompatibleVersion => write!(f, "Index version is not compatible"),
            IndexError::BadTile => write!(f, "Corrupt tile data"),
            IndexError::CorruptFile => write!(f, "Corrupt file"),
            IndexError::BadImage(e) => write!(f, "Corrupt image: {e}"),
            IndexError::BadString(e) => write!(f, "Corrupt string: {e}"),
        }
    }
}

impl error::Error for IndexError {
    fn cause(&self) -> Option<&dyn error::Error> {
        match self {
            IndexError::IoError(e) => Some(e),
            IndexError::ConversionError(e) => Some(e),
            IndexError::BadString(e) => Some(e),
            _ => None,
        }
    }
}

pub type IndexResult<T> = Result<T, IndexError>;
