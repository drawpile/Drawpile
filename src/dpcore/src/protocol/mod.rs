// This file is part of Drawpile.
// Copyright (C) 2020 Calle Laakkonen
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

pub mod message;
pub mod aclfilter;
mod protover;
mod reader;
mod serialization;
mod textmessage;
mod textparser;
mod writer;

pub use message::{Message, VERSION};
pub use protover::ProtocolVersion;
pub use reader::{
    open_recording, BinaryReader, Compatibility, ReadMessage, RecordingReader, TextReader,
};
pub use serialization::{DeserializationError, MessageWriter, MessageReader};
pub use textparser::TextParser;
pub use writer::{BinaryWriter, RecordingWriter, TextWriter};
