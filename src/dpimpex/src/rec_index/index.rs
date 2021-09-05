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

impl fmt::Display for IndexError {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match self {
            IndexError::IoError(e) => write!(f, "IO error: {}", e),
            IndexError::ConversionError(e) => write!(f, "Number too big! ({})", e),
        }
    }
}

impl error::Error for IndexError {
    fn cause(&self) -> Option<&dyn error::Error> {
        match self {
            IndexError::IoError(e) => Some(e),
            IndexError::ConversionError(e) => Some(e),
        }
    }
}

type IndexResult<T> = Result<T, IndexError>;

