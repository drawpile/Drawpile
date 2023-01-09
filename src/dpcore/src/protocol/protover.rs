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

use std::fmt;

#[derive(Debug, Eq, Ord, PartialEq, PartialOrd)]
pub struct ProtocolVersion {
    pub ns: String,
    pub server: i32,
    pub major: i32,
    pub minor: i32,
}

impl ProtocolVersion {
    pub fn from_string(s: &str) -> Option<ProtocolVersion> {
        if let Some(ns_delim) = s.find(':') {
            let ns = &s[..ns_delim];
            let parts: Vec<&str> = s[ns_delim + 1..].split('.').collect();
            if parts.len() == 3 {
                return Some(ProtocolVersion {
                    ns: ns.to_string(),
                    server: parts[0].parse().ok()?,
                    major: parts[1].parse().ok()?,
                    minor: parts[2].parse().ok()?,
                });
            }
        }
        None
    }
}

impl Default for ProtocolVersion {
    fn default() -> Self {
        Self {
            ns: "dp".into(),
            server: 4,
            major: 21,
            minor: 2,
        }
    }
}

impl fmt::Display for ProtocolVersion {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(
            f,
            "{}:{}.{}.{}",
            self.ns, self.server, self.major, self.minor
        )
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_parsing() {
        assert_eq!(
            ProtocolVersion::from_string("dp:4.21.0"),
            Some(ProtocolVersion {
                ns: "dp".to_string(),
                server: 4,
                major: 21,
                minor: 0,
            })
        );
        assert_eq!(ProtocolVersion::from_string("4.21.0"), None);
    }

    #[test]
    fn test_ordering() {
        let a_less_b = [(":0.9.9", ":1.0.0"), (":1.0.0", ":1.0.1")];

        for (a, b) in a_less_b.iter() {
            assert!(ProtocolVersion::from_string(a) < ProtocolVersion::from_string(b));
        }
    }
}
