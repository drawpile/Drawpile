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

use std::str::FromStr;
use std::{fmt, mem};

use super::textmessage::TextMessage;

pub struct TextParser {
    msg: Option<TextMessage>,
    state: State,
    linenum: u32,
}

enum State {
    ExpectCommand,
    ExpectKwargLine,
}

#[derive(PartialEq)]
pub enum ParseResult {
    Ok(TextMessage),
    Metadata(String, String),
    Skip,
    Error(String),
    NeedMore,
}

impl ParseResult {
    pub fn is_error(&self) -> bool {
        matches!(self, ParseResult::Error(_))
    }
}

impl fmt::Debug for ParseResult {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            ParseResult::Ok(m) => write!(f, "Ok({m})"),
            ParseResult::Metadata(k, v) => write!(f, "Metadata({k}={v})"),
            ParseResult::Skip => write!(f, "Skip"),
            ParseResult::Error(e) => write!(f, "Error({e})"),
            ParseResult::NeedMore => write!(f, "NeedMore"),
        }
    }
}

impl FromStr for TextMessage {
    type Err = String;

    fn from_str(message: &str) -> Result<Self, Self::Err> {
        let mut parser = TextParser::new();
        for line in message.lines() {
            match parser.parse_line(line.trim()) {
                ParseResult::Ok(m) => return Ok(m),
                ParseResult::Error(e) => return Err(e),
                _ => {}
            }
        }
        Err("Truncated message".to_string())
    }
}

impl TextParser {
    pub fn new() -> TextParser {
        TextParser {
            msg: None,
            state: State::ExpectCommand,
            linenum: 0,
        }
    }

    pub fn parse_line(&mut self, line: &str) -> ParseResult {
        self.linenum += 1;

        use State::*;
        match self.state {
            ExpectCommand => {
                if line.is_empty() || line.starts_with('#') {
                    return ParseResult::Skip;
                }

                if line.starts_with('!') {
                    // Metadata line
                    if let Some(eq) = line.find('=') {
                        let key = line[1..eq].trim();
                        let value = line[eq + 1..].trim();
                        return ParseResult::Metadata(key.to_string(), value.to_string());
                    }
                    return ParseResult::Error(format!("{}: invalid metadata line", self.linenum));
                }

                let mut tokens = line.split_whitespace();
                let user_id: u8;
                let msg_name: String;

                // First, expect user ID
                if let Some(userid) = tokens.next() {
                    if let Ok(i) = userid.parse::<u8>() {
                        user_id = i;
                    } else {
                        return ParseResult::Error(format!(
                            "{}: Invalid user ID: {}",
                            self.linenum, userid
                        ));
                    }
                } else {
                    return ParseResult::Error(format!("{}: Expected user ID", self.linenum));
                }

                // Next the command name
                if let Some(name) = tokens.next() {
                    msg_name = name.to_string();
                } else {
                    return ParseResult::Error(format!("{}: Expected message name", self.linenum));
                }

                let mut msg = TextMessage::new(user_id, msg_name);

                // Remaining tokens (if any) are arguments
                for token in tokens {
                    if let Some(eq) = token.find('=') {
                        let key = &token[..eq];
                        let value = &token[eq + 1..];
                        msg = msg.set(key.to_string(), value.to_string());
                    } else if token == "{" {
                        // Start of a multiline block
                        self.state = ExpectKwargLine
                    } else {
                        return ParseResult::Error(format!(
                            "{}: Unexpected token: {}",
                            self.linenum, token
                        ));
                    }
                }

                self.msg = Some(msg);
            }
            ExpectKwargLine => {
                if line.is_empty() {
                    return ParseResult::NeedMore;
                } else if line == "}" {
                    self.state = ExpectCommand;
                } else if let Some(eq) = line.find('=') {
                    let key = &line[..eq];
                    let value = &line[eq + 1..];
                    if let Some(m) = self.msg.as_mut() {
                        if let Some(old) = m.args.get_mut(key) {
                            old.push('\n');
                            old.push_str(value);
                        } else {
                            m.args.insert(key.to_string(), value.to_string());
                        }
                    }
                } else {
                    // Expect this to be a dab line
                    let mut dab: Vec<f64> = Vec::new();
                    for token in line.split_whitespace() {
                        match token.parse::<f64>() {
                            Ok(val) => dab.push(val),
                            Err(e) => {
                                self.state = ExpectCommand;
                                self.msg = None;
                                return ParseResult::Error(format!(
                                    "{}: parse error {}",
                                    self.linenum, e
                                ));
                            }
                        }
                    }
                    if let Some(m) = self.msg.as_mut() {
                        m.dabs.push(dab);
                    }
                }
            }
        }

        match self.state {
            ExpectKwargLine => ParseResult::NeedMore,
            ExpectCommand => ParseResult::Ok(mem::replace(&mut self.msg, None).unwrap()),
        }
    }
}

impl Default for TextParser {
    fn default() -> Self {
        Self::new()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_simple() {
        let mut p = TextParser::new();
        let r = p.parse_line("1 hello");
        assert_eq!(r, ParseResult::Ok(TextMessage::new(1, "hello")));
    }

    #[test]
    fn test_blank() {
        let mut p = TextParser::new();
        let r = p.parse_line("");
        assert_eq!(r, ParseResult::Skip);
    }

    #[test]
    fn test_invalid() {
        let mut p = TextParser::new();
        assert!(p.parse_line("hello").is_error());
        assert!(p.parse_line("x hello").is_error());
        assert!(p.parse_line("1 hello x").is_error());
    }

    #[test]
    fn test_multiline() {
        let lines = [
            "!metadata=value",
            "# comment",
            "",
            "1 greeting hello=world {",
            "",
            "test=123",
            "=",
            "=blank_key", // appends to the above
            "empty=",
            "10.5 20 30",
            "}",
            "2 hello",
            "",
        ];

        let expected = [
            ParseResult::Metadata("metadata".to_string(), "value".to_string()),
            ParseResult::Skip,
            ParseResult::Skip,
            ParseResult::NeedMore,
            ParseResult::NeedMore,
            ParseResult::NeedMore,
            ParseResult::NeedMore,
            ParseResult::NeedMore,
            ParseResult::NeedMore,
            ParseResult::NeedMore,
            ParseResult::Ok(
                TextMessage::new(1, "greeting")
                    .set("test", "123".to_string())
                    .set("hello", "world".to_string())
                    .set("", "\nblank_key".to_string())
                    .set("empty", String::new())
                    .set_dabs(vec![vec![10.5, 20.0, 30.0]]),
            ),
            ParseResult::Ok(TextMessage::new(2, "hello")),
            ParseResult::Skip,
        ];

        let mut p = TextParser::new();
        for (line, ex) in lines.iter().zip(expected.iter()) {
            let r = p.parse_line(line);
            assert_eq!(r, *ex);
        }
    }

    #[test]
    fn test_parse() {
        let valid = "1 msg {\nhello=world\n}";

        assert_eq!(
            valid.parse(),
            Ok(TextMessage::new(1, "msg").set("hello", "world".to_string()))
        );

        let invalid = "1 msg {\nx\n}";
        assert!(TextMessage::from_str(invalid).is_err());

        let truncated = "1 msg {";
        assert!(TextMessage::from_str(truncated).is_err());
    }
}
