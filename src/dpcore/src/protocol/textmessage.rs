// This file is part of Drawpile.
// Copyright (C) 2020-2022 Calle Laakkonen
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

use crate::paint::Blendmode;
use base64;
use std::collections::BTreeMap;
use std::convert::TryFrom;
use std::{fmt, str};

#[derive(PartialEq, Debug)]
pub struct TextMessage {
    pub user_id: u8,
    pub name: String,
    pub args: BTreeMap<String, String>,
    pub dabs: Vec<Vec<f64>>,
}

impl TextMessage {
    pub fn new<T: Into<String>>(user_id: u8, name: T) -> TextMessage {
        TextMessage {
            user_id,
            name: name.into(),
            args: BTreeMap::new(),
            dabs: Vec::new(),
        }
    }

    pub fn set_dabs(mut self, dabs: Vec<Vec<f64>>) -> Self {
        self.dabs = dabs;
        self
    }

    pub fn set<T: Into<String>>(mut self, key: T, value: String) -> Self {
        self.args.insert(key.into(), value);
        self
    }

    pub fn get_str(&self, key: &str) -> &str {
        self.args.get(key).map(String::as_str).unwrap_or("")
    }

    pub fn get_u8(&self, key: &str) -> u8 {
        let v = self.get_str(key);
        (if let Some(hex) = v.strip_prefix("0x") {
            u8::from_str_radix(hex, 16)
        } else {
            v.parse()
        })
        .unwrap_or_default()
    }

    pub fn get_u16(&self, key: &str) -> u16 {
        let v = self.get_str(key);
        (if let Some(hex) = v.strip_prefix("0x") {
            u16::from_str_radix(hex, 16)
        } else {
            v.parse()
        })
        .unwrap_or_default()
    }

    pub fn get_u32(&self, key: &str) -> u32 {
        let v = self.get_str(key);
        (if let Some(hex) = v.strip_prefix("0x") {
            u32::from_str_radix(hex, 16)
        } else {
            v.parse()
        })
        .unwrap_or_default()
    }

    pub fn get_f64(&self, key: &str) -> f64 {
        self.get_str(key).parse().unwrap_or_default()
    }

    /// Set a byte string.
    /// The byte string will be base64 encoded
    pub fn set_bytes<T: Into<String>>(mut self, key: T, value: &[u8]) -> Self {
        let buf = base64::encode_config(value, base64::STANDARD_NO_PAD);

        let mut s = String::new();
        for chunk in buf.as_bytes().chunks(70) {
            s.push_str(str::from_utf8(chunk).unwrap());
            s.push('\n');
        }

        self.args.insert(key.into(), s);
        self
    }

    /// Get a byte string from a base64 encoded value
    pub fn get_bytes(&self, key: &str) -> Vec<u8> {
        let mut buf = self.get_str(key).to_string();
        buf.retain(|c| !c.is_whitespace());
        base64::decode_config(&buf, base64::STANDARD_NO_PAD).unwrap_or_default()
    }

    /// Set a vector of u8 integers as a comma separated list
    pub fn set_vec_u8<T: Into<String>>(mut self, key: T, value: &[u8]) -> Self {
        let buf = value
            .iter()
            .map(|v| v.to_string())
            .collect::<Vec<String>>()
            .join(",");
        self.args.insert(key.into(), buf);
        self
    }

    /// Get a vector of u8 values
    /// Invalid values are skipped
    pub fn get_vec_u8(&self, key: &str) -> Vec<u8> {
        self.get_str(key)
            .split(',')
            .filter_map(|v| {
                if let Some(hex) = v.strip_prefix("0x") {
                    u8::from_str_radix(hex, 16).ok()
                } else {
                    v.parse::<u8>().ok()
                }
            })
            .collect()
    }

    /// Set a vector of u16 integers as a comma separated list
    /// If the hex parameter is true, the numbers will be in hexadecimal.
    /// (this makes things like layer IDs more readable)
    pub fn set_vec_u16<T: Into<String>>(mut self, key: T, value: &[u16], hex: bool) -> Self {
        let buf = value
            .iter()
            .map(|v| {
                if hex {
                    format!("0x{:04x}", v)
                } else {
                    v.to_string()
                }
            })
            .collect::<Vec<String>>()
            .join(",");
        self.args.insert(key.into(), buf);
        self
    }

    /// Get a vector of u8 values
    /// Invalid values are skipped
    pub fn get_vec_u16(&self, key: &str) -> Vec<u16> {
        self.get_str(key)
            .split(',')
            .filter_map(|v| {
                if let Some(hex) = v.strip_prefix("0x") {
                    u16::from_str_radix(hex, 16).ok()
                } else {
                    v.parse::<u16>().ok()
                }
            })
            .collect()
    }

    pub fn set_blendmode<T: Into<String>>(mut self, key: T, mode: u8) -> Self {
        self.args.insert(
            key.into(),
            Blendmode::try_from(mode)
                .unwrap_or_default()
                .svg_name()
                .to_string(),
        );
        self
    }

    pub fn get_blendmode(&self, key: &str) -> u8 {
        Blendmode::from_svg_name(self.get_str(key))
            .unwrap_or_default()
            .into()
    }

    pub fn set_argb32<T: Into<String>>(mut self, key: T, color: u32) -> Self {
        self.args.insert(
            key.into(),
            if (color & 0xff_000000) != 0xff_000000 {
                format!("#{:08x}", color)
            } else {
                format!("#{:06x}", color & 0x00_ffffff)
            },
        );
        self
    }

    pub fn get_argb32(&self, key: &str) -> u32 {
        let color = self.get_str(key);
        if !color.starts_with('#') || (color.len() != 7 && color.len() != 9) {
            return 0;
        }

        let v = u32::from_str_radix(&color[1..], 16).unwrap_or(0);
        if color.len() == 7 {
            v | 0xff_000000
        } else {
            v
        }
    }

    pub fn set_flags<T: Into<String>>(
        mut self,
        key: T,
        flags: &[&'static str],
        bitfield: u8,
    ) -> Self {
        debug_assert!(flags.len() <= 8);
        self.args.insert(
            key.into(),
            (0..flags.len())
                .filter_map(|bit| -> Option<&'static str> {
                    debug_assert!(!flags[bit].contains(','));
                    if (bitfield & (1 << bit)) != 0 {
                        Some(flags[bit])
                    } else {
                        None
                    }
                })
                .collect::<Vec<&'static str>>()
                .join(","),
        );
        self
    }

    pub fn get_flags(&self, flags: &[&'static str], key: &str) -> u8 {
        debug_assert!(flags.len() <= 8);

        let val = self.get_str(key);
        let mut result = 0u8;

        for flag in val.split(',') {
            if let Some(bit) = flags.iter().position(|&f| f == flag) {
                result |= 1 << bit;
            }
        }

        result
    }
}

impl fmt::Display for TextMessage {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{} {}", self.user_id, self.name)?;

        let mut has_multiline = !self.dabs.is_empty();

        for (key, value) in &self.args {
            if value.chars().any(char::is_whitespace) {
                has_multiline = true;
            } else if !value.is_empty() {
                write!(f, " {}={}", key, value)?;
            }
        }

        if has_multiline {
            f.write_str(" {\n")?;
            for (key, value) in &self.args {
                if value.chars().any(char::is_whitespace) {
                    for line in value.lines() {
                        writeln!(f, "    {}={}", key, line)?;
                    }
                }
            }

            for dab in self.dabs.iter() {
                write!(f, "   ")?;
                for c in dab.iter() {
                    write!(f, " {}", c)?;
                }
                writeln!(f)?;
            }
            f.write_str("}")?;
        }

        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_tostring() {
        let m = TextMessage::new(1, "demo")
            .set("hello", String::from("world"))
            .set("mll", String::from("contains spaces and\nnewlines"));

        assert_eq!(
            m.to_string(),
            "1 demo hello=world {\n    mll=contains spaces and\n    mll=newlines\n}"
        );
    }

    #[test]
    fn test_binary() {
        let bin = b"Hello world. Lorem ipsum dolor sit amet and so on. This line should be long enough to get split.";
        let m = TextMessage::new(1, "demo").set_bytes("hello", bin);

        assert_eq!(m.get_bytes("hello"), bin.to_vec());
    }

    #[test]
    fn test_vec_u8() {
        let vec: Vec<u8> = vec![1, 2, 3, 30, 0xff];
        let m = TextMessage::new(1, "demo").set_vec_u8("hello", &vec);

        assert_eq!(m.args.get("hello"), Some(&String::from("1,2,3,30,255")));
        assert_eq!(m.get_vec_u8("hello"), vec);

        let m = m.set("hello", String::from("0x01,,2,x,3,512"));
        assert_eq!(m.get_vec_u8("hello"), vec![1, 2, 3]);
    }

    #[test]
    fn test_vec_u16() {
        let vec: Vec<u16> = vec![0x0102, 0x0200, 0x0305, 0x80ff, 0xffff];
        let m = TextMessage::new(1, "demo").set_vec_u16("hello", &vec, false);

        assert_eq!(
            m.args.get("hello"),
            Some(&String::from("258,512,773,33023,65535"))
        );
        assert_eq!(m.get_vec_u16("hello"), vec);

        let m = m.set_vec_u16("hello", &vec, true);
        assert_eq!(
            m.args.get("hello"),
            Some(&String::from("0x0102,0x0200,0x0305,0x80ff,0xffff"))
        );
        assert_eq!(m.get_vec_u16("hello"), vec);
    }

    #[test]
    fn test_argb32() {
        let m = TextMessage::new(1, "demo").set_argb32("c", 0xff112233);
        assert_eq!(m.get_str("c"), "#112233");
        assert_eq!(m.get_argb32("c"), 0xff112233);

        let m = m.set_argb32("c", 0x80112233);
        assert_eq!(m.get_str("c"), "#80112233");
        assert_eq!(m.get_argb32("c"), 0x80112233);
    }

    #[test]
    fn test_flags() {
        let flags = ["A", "B", "C"];

        let m = TextMessage::new(1, "demo").set_flags("f", &flags, 0x1);
        assert_eq!(m.args.get("f"), Some(&String::from("A")));
        assert_eq!(m.get_flags(&flags, "f"), 0x01);

        let m = m.set_flags("f", &flags, 0x03);
        assert_eq!(m.get_str("f"), "A,B");
        assert_eq!(m.get_flags(&flags, "f"), 0x03);
    }

    #[test]
    fn test_dabs() {
        let m = TextMessage::new(1, "demo")
            .set_argb32("c", 1)
            .set_dabs(vec![vec![1.25, 0.0, 0.0], vec![2.0, 1.0, 3.0]]);
        assert_eq!(
            m.to_string(),
            "1 demo c=#00000001 {\n    1.25 0 0\n    2 1 3\n}"
        );
    }
}
