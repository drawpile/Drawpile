// This file is part of Drawpile.
// Copyright (C) 2021 Calle Laakkonen
//
// Drawpile is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// As additional permission under section 7, you are allowed to distribute
// the software through an app store, even if that store has restrictive
// terms and conditions that are incompatible with the GPL, provided that
// the source is also available under the GPL with or without this permission
// through a channel without those restrictive terms and conditions.
//
// Drawpile is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Drawpile.  If not, see <https://www.gnu.org/licenses/>.

use dpcore::paint::LayerStack;
use image::error::ImageError;
use std::path::Path;
use std::{fmt, io};
use xml::writer::Error as XmlError;
use zip::result::ZipError;

pub mod animation;
pub mod conv;
mod flat;
mod ora_reader;
mod ora_utils;
mod ora_writer;
pub mod rec;
pub mod rec_index;

#[derive(Debug)]
pub enum ImpexError {
    IoError(io::Error),
    CodecError(ImageError),
    UnsupportedFormat,
    XmlError(XmlError),
    NoContent,
}

impl fmt::Display for ImpexError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            ImpexError::IoError(e) => e.fmt(f),
            ImpexError::CodecError(e) => e.fmt(f),
            ImpexError::UnsupportedFormat => write!(f, "unsupported format"),
            ImpexError::NoContent => write!(f, "no content"),
            ImpexError::XmlError(e) => e.fmt(f),
        }
    }
}

impl std::error::Error for ImpexError {
    fn cause(&self) -> Option<&dyn std::error::Error> {
        match self {
            ImpexError::IoError(e) => Some(e),
            ImpexError::CodecError(e) => Some(e),
            ImpexError::XmlError(e) => Some(e),
            _ => None,
        }
    }
}

impl From<io::Error> for ImpexError {
    fn from(err: io::Error) -> Self {
        Self::IoError(err)
    }
}

impl From<ImageError> for ImpexError {
    fn from(err: ImageError) -> Self {
        Self::CodecError(err)
    }
}

impl From<ZipError> for ImpexError {
    fn from(err: ZipError) -> Self {
        match err {
            ZipError::Io(io) => Self::IoError(io),
            _ => Self::UnsupportedFormat,
        }
    }
}

impl From<XmlError> for ImpexError {
    fn from(err: XmlError) -> Self {
        Self::XmlError(err)
    }
}

pub type ImageImportResult = Result<LayerStack, ImpexError>;
pub type ImageExportResult = Result<(), ImpexError>;

pub fn load_image<P>(path: P) -> ImageImportResult
where
    P: AsRef<Path>,
{
    fn inner(path: &Path) -> ImageImportResult {
        let ext = path
            .extension()
            .and_then(|s| s.to_str())
            .and_then(|s| Some(s.to_ascii_lowercase()));
        match ext.as_deref() {
            Some("ora") => ora_reader::load_openraster_image(path),
            Some("gif") => flat::load_gif_animation(path),
            Some(_) => flat::load_flat_image(path),
            None => Err(ImpexError::UnsupportedFormat),
        }
    }
    inner(path.as_ref())
}

pub fn save_image<P>(path: P, layerstack: &LayerStack) -> ImageExportResult
where
    P: AsRef<Path>,
{
    fn inner(path: &Path, layerstack: &LayerStack) -> ImageExportResult {
        let ext = path
            .extension()
            .and_then(|s| s.to_str())
            .and_then(|s| Some(s.to_ascii_lowercase()));
        match ext.as_deref() {
            Some("ora") => ora_writer::save_openraster_image(path, layerstack),
            Some(_) => flat::save_flat_image(path, layerstack),
            None => Err(ImpexError::UnsupportedFormat),
        }
    }
    inner(path.as_ref(), layerstack)
}
