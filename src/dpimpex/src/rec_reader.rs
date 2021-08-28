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

use serde_json;
use std::collections::HashMap;
use std::convert::TryInto;
use std::fs::File;
use std::io;
use std::io::{BufRead, BufReader, Read, Seek, SeekFrom};
use std::str;

use dpcore::protocol::textparser::{ParseResult, TextParser};
use dpcore::protocol::{Message, ProtocolVersion, PROTOCOL_VERSION, HEADER_LEN};

#[derive(Debug)]
pub enum ReadMessage {
    Ok(Message),
    Invalid(String),
    IoError(io::Error),
    Eof,
}

#[derive(PartialEq)]
pub enum Compatibility {
    /// Recording is either the same format or a known compatible version
    Compatible,

    /// Expect minor rendering differences
    MinorDifferences,

    /// Unknown compatibility. Might work fully, might not work at all.
    Unknown,

    /// Known to be incompatible
    Incompatible,
}

pub trait RecordingReader {
    /// Get a header metadata value
    ///
    /// Typical keys of interest:
    /// - version: the protocol version
    /// - writerversion: version string of the application that made the recording
    fn get_metadata(&self, key: &str) -> Option<&String>;

    /// Get all header metadata
    fn get_metadata_all(&self) -> &HashMap<String, String>;

    /// Check the recordings version number against the current protocol version
    fn check_compatibility(&self) -> Compatibility {
        if let Some(vstr) = self.get_metadata("version") {
            let our = ProtocolVersion::from_string(PROTOCOL_VERSION).unwrap();
            if let Some(their) = ProtocolVersion::from_string(vstr) {
                compare_versions(&our, &their)
            } else {
                // Probably not compatible if we can't even parse the version string
                Compatibility::Incompatible
            }
        } else {
            Compatibility::Unknown
        }
    }

    /// Read a message from the file
    fn read_next(&mut self) -> ReadMessage;

    /// Return the index of the last read message (first message is at zero)
    fn current_index(&self) -> usize;
}

fn compare_versions(our: &ProtocolVersion, their: &ProtocolVersion) -> Compatibility {
    if our == their {
        Compatibility::Compatible
    } else if our.ns != their.ns {
        // Different namespace, unknown version: incompatible is the safe bet
        Compatibility::Incompatible
    } else if our.major < their.major {
        // Newer major version: likely contains new unsupported commands
        Compatibility::Unknown
    } else if their.major < 21 {
        // Known to be incompatible
        Compatibility::Incompatible
    } else {
        // Different minor version
        Compatibility::MinorDifferences
    }
}

/// Open a recording file, guessing its type based on the contents
pub fn open_recording(filename: &str) -> io::Result<Box<dyn RecordingReader>> {
    let mut file = File::open(filename)?;

    let mut sample = [0; 512];
    let sample_size = file.read(&mut sample)?;

    if sample_size < 6 {
        return Err(io::Error::new(
            io::ErrorKind::Other,
            "File is too short to be a recording",
        ));
    }

    file.seek(io::SeekFrom::Start(0))?;

    if &sample[0..6] == b"DPREC\0" {
        // This looks like a binary recording
        return Ok(Box::new(BinaryReader::open(file)?));
    }

    // Check if this looks like a text recording
    str::from_utf8(&sample).map_err(|_| {
        io::Error::new(
            io::ErrorKind::Other,
            "Input is not a binary recording, but does not look like a text file either!",
        )
    })?;

    Ok(Box::new(TextReader::open(BufReader::new(file))?))
}

pub struct BinaryReader<R> {
    file: R,
    last_read_index: usize,
    metadata: HashMap<String, String>,
    read_buffer: [u8; 0xffff + HEADER_LEN],
}

impl<R: Read> BinaryReader<R> {
    pub fn open(file: R) -> io::Result<BinaryReader<R>> {
        let mut br = BinaryReader {
            file,
            last_read_index: 0,
            metadata: HashMap::new(),
            read_buffer: [0; 0xffff + HEADER_LEN],
        };

        // A binary recording starts with the magic number
        let mut magic: [u8; 6] = [0; 6];
        br.file.read_exact(&mut magic)?;

        if magic != *b"DPREC\0" {
            return Err(io::Error::new(io::ErrorKind::Other, "Not a DPREC file!"));
        }

        // Followed by a JSON metadata block
        let mut metadata_len_buf: [u8; 2] = [0, 0];
        br.file.read_exact(&mut metadata_len_buf)?;

        let metadata_len = u16::from_be_bytes(metadata_len_buf);

        let mut metadata: Vec<u8> = Vec::new();
        metadata.resize(metadata_len as usize, 0);
        br.file.read_exact(&mut metadata)?;

        let jsondata = serde_json::from_slice(&metadata)
            .map_err(|e| io::Error::new(io::ErrorKind::Other, e.to_string()))?;

        if let serde_json::Value::Object(entries) = jsondata {
            for (key, value) in entries.iter() {
                if value.is_string() {
                    br.metadata
                        .insert(key.to_string(), value.as_str().unwrap().to_string());
                }
            }
        } else {
            return Err(io::Error::new(
                io::ErrorKind::Other,
                "Header did not contain a JSON object!",
            ));
        }
        Ok(br)
    }

    fn read_message(reader: &mut R, buf: &mut [u8]) -> io::Result<usize> {
        reader.read_exact(&mut buf[..HEADER_LEN])?;
        let payload_len = u16::from_be_bytes(buf[..2].try_into().unwrap()) as usize;
        if payload_len > 0 {
            reader.read_exact(&mut buf[HEADER_LEN..HEADER_LEN + payload_len])?;
        }

        Ok(payload_len + HEADER_LEN)
    }

    pub fn read_next_raw(&mut self, buf: &mut [u8]) -> io::Result<usize> {
        Self::read_message(&mut self.file, buf).and_then(|v| {
            self.last_read_index += 1;
            Ok(v)
        })
    }
}

impl<R: Read> RecordingReader for BinaryReader<R> {
    fn read_next(&mut self) -> ReadMessage {
        let msg_len;
        match Self::read_message(&mut self.file, &mut self.read_buffer) {
            Ok(size) => {
                msg_len = size;
            }
            Err(e) => {
                return if e.kind() == io::ErrorKind::UnexpectedEof {
                    ReadMessage::Eof
                } else {
                    ReadMessage::IoError(e)
                }
            }
        }

        match Message::deserialize(&self.read_buffer[..msg_len]) {
            Ok(m) => {
                self.last_read_index += 1;
                ReadMessage::Ok(m)
            }
            Err(e) => ReadMessage::Invalid(format!("Deserialization error: {:?}", e)),
        }
    }

    fn current_index(&self) -> usize {
        self.last_read_index - 1
    }

    fn get_metadata(&self, key: &str) -> Option<&String> {
        self.metadata.get(key)
    }

    fn get_metadata_all(&self) -> &HashMap<String, String> {
        &self.metadata
    }
}

pub struct TextReader<R> {
    file: R,
    last_read_index: usize,
    metadata: HashMap<String, String>,
    parser: TextParser,
    line_buf: String,
}

impl<R: BufRead + Seek> TextReader<R> {
    pub fn open(file: R) -> io::Result<TextReader<R>> {
        let mut br = TextReader {
            file,
            last_read_index: 0,
            metadata: HashMap::new(),
            parser: TextParser::new(),
            line_buf: String::new(),
        };

        // Read the metadata (until the first command)
        let mut metadata_parser = TextParser::new();
        loop {
            br.line_buf.truncate(0);
            if br.file.read_line(&mut br.line_buf)? == 0 {
                break;
            }

            match metadata_parser.parse_line(&br.line_buf.trim()) {
                ParseResult::Ok(_) | ParseResult::NeedMore | ParseResult::Error(_) => {
                    break;
                }
                ParseResult::Skip => {}
                ParseResult::Metadata(k, v) => {
                    br.metadata.insert(k, v);
                }
            }
        }

        br.file.seek(SeekFrom::Start(0))?;
        Ok(br)
    }
}

impl<R: BufRead> RecordingReader for TextReader<R> {
    fn read_next(&mut self) -> ReadMessage {
        loop {
            self.line_buf.truncate(0);
            match self.file.read_line(&mut self.line_buf) {
                Ok(n) => {
                    if n == 0 {
                        return ReadMessage::Eof;
                    }
                }
                Err(e) => {
                    return ReadMessage::IoError(e);
                }
            }

            match self.parser.parse_line(&self.line_buf.trim()) {
                ParseResult::Ok(tm) => match Message::from_text(&tm) {
                    Some(m) => {
                        self.last_read_index += 1;
                        return ReadMessage::Ok(m);
                    }
                    None => return ReadMessage::Invalid(format!("Unknown message: {}", tm.name)),
                },
                ParseResult::Skip | ParseResult::NeedMore | ParseResult::Metadata(_, _) => {}
                ParseResult::Error(e) => {
                    return ReadMessage::Invalid(e);
                }
            }
        }
    }

    fn current_index(&self) -> usize {
        self.last_read_index - 1
    }

    fn get_metadata(&self, key: &str) -> Option<&String> {
        self.metadata.get(key)
    }

    fn get_metadata_all(&self) -> &HashMap<String, String> {
        &self.metadata
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use dpcore::protocol::message::*;
    use std::io::Cursor;

    fn test_reader(reader: &mut dyn RecordingReader) {
        assert_eq!(reader.get_metadata("hello").expect("hello header"), "world");
        assert_eq!(
            reader.get_metadata("version").expect("version header"),
            "1.0"
        );

        if let ReadMessage::Ok(m) = reader.read_next() {
            assert_eq!(
                m,
                ServerMetaMessage::Join(
                    1,
                    JoinMessage {
                        flags: 0x01,
                        name: "ABC".to_string(),
                        avatar: Vec::new(),
                    }
                )
                .into()
            );
        } else {
            panic!("Message deserialization failed!");
        }

        assert_eq!(reader.current_index(), 0);

        assert!(match reader.read_next() {
            ReadMessage::Eof => true,
            x => panic!("Got: {:?}", x),
        });

        assert_eq!(reader.current_index(), 0);
    }

    #[test]
    fn test_binary_reader() {
        let testdata =
            b"DPREC\0\0\x24{\"hello\": \"world\", \"version\": \"1.0\"}\0\x05\x20\x01\x01\x03ABC";

        test_reader(&mut BinaryReader::open(&testdata[..]).unwrap());
    }

    #[test]
    fn test_text_reader() {
        let testdata = br#"
        !version=1.0
        !hello=world

        1 join name=ABC flags=auth
        "#;

        test_reader(&mut TextReader::open(Cursor::new(&testdata[..])).unwrap());
    }
}
