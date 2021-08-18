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

use std::convert::TryInto;
use std::mem;

pub const HEADER_LEN: usize = 4;

#[derive(Debug)]
pub struct DeserializationError {
    pub user_id: u8,
    pub message_type: u8,
    pub payload_len: usize,
    pub error: &'static str,
}

pub trait Serializable {
    fn write(&self, v: &mut Vec<u8>);
    fn len(&self) -> usize;
}

pub trait DeserializableScalar {
    fn read(v: &[u8]) -> Self;
}

impl Serializable for u8 {
    fn write(&self, v: &mut Vec<u8>) {
        v.push(*self);
    }
    fn len(&self) -> usize {
        mem::size_of::<Self>()
    }
}

impl DeserializableScalar for u8 {
    fn read(v: &[u8]) -> Self {
        v[0]
    }
}

impl Serializable for i8 {
    fn write(&self, v: &mut Vec<u8>) {
        v.push(*self as u8);
    }
    fn len(&self) -> usize {
        mem::size_of::<Self>()
    }
}

impl DeserializableScalar for i8 {
    fn read(v: &[u8]) -> Self {
        v[0] as i8
    }
}

impl Serializable for bool {
    fn write(&self, v: &mut Vec<u8>) {
        v.push(if *self { 1 } else { 0 });
    }
    fn len(&self) -> usize {
        1
    }
}

impl DeserializableScalar for bool {
    fn read(v: &[u8]) -> Self {
        v[0] != 0
    }
}

impl Serializable for u16 {
    fn write(&self, v: &mut Vec<u8>) {
        v.extend_from_slice(&self.to_be_bytes());
    }
    fn len(&self) -> usize {
        mem::size_of::<Self>()
    }
}

impl DeserializableScalar for u16 {
    fn read(v: &[u8]) -> Self {
        u16::from_be_bytes(v[..2].try_into().unwrap())
    }
}

impl Serializable for u32 {
    fn write(&self, v: &mut Vec<u8>) {
        v.extend_from_slice(&self.to_be_bytes());
    }
    fn len(&self) -> usize {
        mem::size_of::<Self>()
    }
}

impl DeserializableScalar for u32 {
    fn read(v: &[u8]) -> Self {
        u32::from_be_bytes(v[..4].try_into().unwrap())
    }
}

impl Serializable for i32 {
    fn write(&self, v: &mut Vec<u8>) {
        v.extend_from_slice(&self.to_be_bytes());
    }
    fn len(&self) -> usize {
        mem::size_of::<Self>()
    }
}

impl DeserializableScalar for i32 {
    fn read(v: &[u8]) -> Self {
        i32::from_be_bytes(v[..4].try_into().unwrap())
    }
}

impl Serializable for &String {
    fn write(&self, v: &mut Vec<u8>) {
        v.extend_from_slice(&self.as_bytes());
    }

    fn len(&self) -> usize {
        String::len(&self)
    }
}

impl Serializable for &Vec<u8> {
    fn write(&self, v: &mut Vec<u8>) {
        v.extend_from_slice(&self);
    }

    fn len(&self) -> usize {
        Vec::<u8>::len(&self)
    }
}

impl Serializable for &Vec<u16> {
    fn write(&self, v: &mut Vec<u8>) {
        for i in self.iter() {
            v.extend(&i.to_be_bytes())
        }
    }

    fn len(&self) -> usize {
        Vec::<u16>::len(&self) * 2
    }
}

pub struct MessageReader<'a> {
    buf: &'a [u8],
}

impl<'a> MessageReader<'a> {
    pub fn new(buf: &'a [u8]) -> MessageReader<'a> {
        MessageReader { buf }
    }

    pub fn check_len(
        self,
        min_len: usize,
        max_len: usize,
        message_type: u8,
        user_id: u8,
    ) -> Result<MessageReader<'a>, DeserializationError> {
        if self.buf.len() < min_len {
            Err(DeserializationError {
                user_id,
                message_type,
                payload_len: self.buf.len(),
                error: "Payload is shorten than expected minimum length",
            })
        } else if self.buf.len() > max_len {
            Err(DeserializationError {
                user_id,
                message_type,
                payload_len: self.buf.len(),
                error: "Payload is longer than expected maximum length",
            })
        } else {
            Ok(self)
        }
    }

    pub fn remaining(&self) -> usize {
        self.buf.len()
    }

    pub fn read<T: DeserializableScalar>(&mut self) -> T {
        let value = T::read(&self.buf);
        self.buf = &self.buf[mem::size_of::<T>()..];
        value
    }

    pub fn read_vec<T: DeserializableScalar>(&mut self, items: usize) -> Vec<T> {
        let size = mem::size_of::<T>();
        let mut vec = Vec::<T>::with_capacity(items);
        for i in (0..size * items).step_by(size) {
            vec.push(T::read(&self.buf[i..]))
        }
        self.buf = &self.buf[(size * items)..];
        vec
    }

    pub fn read_remaining_vec<T: DeserializableScalar>(&mut self) -> Vec<T> {
        self.read_vec::<T>(self.remaining() / mem::size_of::<T>())
    }

    pub fn read_str(&mut self, len: usize) -> String {
        String::from_utf8(self.read_vec::<u8>(len)).unwrap_or_default()
    }

    pub fn read_remaining_str(&mut self) -> String {
        self.read_str(self.remaining())
    }
}

pub struct MessageWriter {
    buf: Vec<u8>,
    expecting: i32,
}

impl MessageWriter {
    pub fn new() -> MessageWriter {
        MessageWriter {
            buf: Vec::new(),
            expecting: 0,
        }
    }

    /// Write a message whose payload consists of a single value
    pub fn single<T: Serializable>(&mut self, message_type: u8, user_id: u8, value: T) {
        self.write_header(message_type, user_id, value.len());
        self.write(value);
    }

    /// Write a payload value
    pub fn write<T: Serializable>(&mut self, value: T) {
        value.write(&mut self.buf);
        self.expecting -= value.len() as i32;
        assert!(self.expecting >= 0);
    }

    /// Write a message header. Previous message (if any) must have been completed.
    pub fn write_header(&mut self, message_type: u8, user_id: u8, payload_len: usize) {
        assert!(payload_len <= 0xffff);
        assert!(self.expecting == 0);
        // TODO reserve known capacity?
        self.expecting = 4 + payload_len as i32;
        self.write(payload_len as u16);
        self.write(message_type);
        self.write(user_id);
    }

    pub fn as_ptr(&self, len: &mut usize) -> *const u8 {
        *len = self.buf.len();
        self.buf.as_ptr()
    }
}

impl Into<Vec<u8>> for MessageWriter {
    fn into(self) -> Vec<u8> {
        assert!(self.expecting == 0);
        self.buf
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_reader() {
        let test = b"\xff\xa0\xb0\0\0\0\x03\x05hello\x05by\0es\x00\x01\x00\x02world";
        let mut reader = MessageReader::new(&test[..]);

        assert_eq!(reader.read::<u8>(), 0xffu8);
        assert_eq!(reader.read::<u16>(), 0xa0b0u16);
        assert_eq!(reader.read::<u32>(), 3u32);

        let vlen = reader.read::<u8>() as usize;
        assert_eq!(reader.read_str(vlen), "hello");

        let vlen = reader.read::<u8>() as usize;
        assert_eq!(reader.read_vec::<u8>(vlen), b"by\0es");

        assert_eq!(reader.read_vec::<u16>(2), vec![1u16, 2u16]);

        assert_eq!(reader.read_str(reader.remaining()), "world");

        assert_eq!(reader.read_vec::<u8>(reader.remaining()), b"");
    }

    #[test]
    fn test_writer() {
        let mut writer = MessageWriter::new();
        writer.expecting = 28;
        writer.write(0xff_u8); // +1
        writer.write(0xa0b0_u16); // +2
        writer.write(3_u32); // +4
        writer.write(5_u8); // +1
        writer.write(&String::from("hello")); // +5
        writer.write(5_u8); // +1
        writer.write(&b"by\0es".to_vec()); // +5
        writer.write(&[1u16, 2u16].to_vec()); // +4
        writer.write(&String::from("world")); // +5

        let result: Vec<u8> = writer.into();
        let expected = b"\xff\xa0\xb0\0\0\0\x03\x05hello\x05by\0es\x00\x01\x00\x02world";
        assert_eq!(&result, expected);
    }

    #[test]
    fn test_single_writer() {
        let mut w = MessageWriter::new();
        w.single(1, 2, &String::from("Hello"));
        let msg: Vec<u8> = w.into();

        let mut reader = MessageReader::new(&msg);
        assert_eq!(reader.read::<u16>(), 5);
        assert_eq!(reader.read::<u8>(), 1);
        assert_eq!(reader.read::<u8>(), 2);
        assert_eq!(reader.read_str(5), "Hello");
    }
}
