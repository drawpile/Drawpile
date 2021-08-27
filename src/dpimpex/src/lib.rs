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
use std::io;
use std::path::Path;
use zip::result::ZipError;

mod conv;
mod flat;
mod ora_reader;
mod ora_utils;

#[derive(Debug)]
pub enum ImageImportError {
    IoError(io::Error),
    DecodeError(ImageError),
    UnsupportedFormat,
    NoContent,
}

impl From<io::Error> for ImageImportError {
    fn from(err: io::Error) -> Self {
        Self::IoError(err)
    }
}

impl From<ImageError> for ImageImportError {
    fn from(err: ImageError) -> Self {
        Self::DecodeError(err)
    }
}

impl From<ZipError> for ImageImportError {
    fn from(err: ZipError) -> Self {
        match err {
            ZipError::Io(io) => Self::IoError(io),
            _ => Self::UnsupportedFormat,
        }
    }
}

pub type ImportResult = Result<LayerStack, ImageImportError>;

pub fn load_image<P>(path: P) -> ImportResult
where
    P: AsRef<Path>,
{
    fn inner(path: &Path) -> ImportResult {
        let ext = path
            .extension()
            .and_then(|s| s.to_str())
            .and_then(|s| Some(s.to_ascii_lowercase()));
        match ext.as_deref() {
            Some("ora") => ora_reader::load_openraster_image(path),
            Some("gif") => flat::load_gif_animation(path),
            Some(_) => flat::load_flat_image(path),
            None => Err(ImageImportError::UnsupportedFormat),
        }
    }
    inner(path.as_ref())
}
