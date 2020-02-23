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

use dpcore::protocol::message::*;

#[test]
fn test_message_serialization() {
    let test_data = vec![
        (
            b"\x00\x0b\x20\x01\x03\x05helloworld".to_vec(),
            Message::from(ServerMetaMessage::Join(
                1,
                JoinMessage {
                    flags: 3,
                    name: "hello".to_string(),
                    avatar: b"world".to_vec(),
                },
            )),
        ),
        (
            b"\x00\x00\x21\x80".to_vec(),
            Message::from(ServerMetaMessage::Leave(128)),
        ),
        (
            b"\x00\x03\x22\xff\x10\x20\x30".to_vec(),
            Message::from(ServerMetaMessage::SessionOwner(
                255,
                b"\x10\x20\x30".to_vec(),
            )),
        ),
        (
            b"\x00\x06\x23\x0a\x05hello".to_vec(),
            Message::from(ServerMetaMessage::Chat(
                10,
                ChatMessage {
                    flags: 0x05,
                    message: "hello".to_string(),
                },
            )),
        ),
        (
            b"\x00\x15\x94\x01\x00\x01\0\0\0\x01\0\0\0\x02\0\0\0\0\x01\x04\x05\x01\x00\xff\x80"
                .to_vec(),
            Message::from(CommandMessage::DrawDabsClassic(
                1,
                DrawDabsClassicMessage {
                    layer: 1,
                    x: 1,
                    y: 2,
                    color: 0,
                    mode: 1,
                    dabs: vec![ClassicDab {
                        x: 4,
                        y: 5,
                        size: 256,
                        opacity: 128,
                        hardness: 255,
                    }],
                },
            )),
        ),
        (
            b"\x00\x10\x81\x01\0\0\0\x0a\xff\xff\xff\xf6\0\0\0\x63\xff\xff\xfe\xbf".to_vec(),
            Message::from(CommandMessage::CanvasResize(
                1,
                CanvasResizeMessage {
                    top: 10,
                    right: -10,
                    bottom: 99,
                    left: -321,
                },
            )),
        ),
    ];

    for (sample, expected) in test_data {
        let msg = Message::deserialize(&sample).unwrap();
        assert_eq!(msg, expected, "deserialization");
        assert_eq!(msg.serialize(), sample, "re-serialization");

        let text = msg.to_string();
        assert_eq!(
            Message::from_text(&text.parse().unwrap()).unwrap(),
            expected,
            "text encode + parsing"
        );
    }
}
