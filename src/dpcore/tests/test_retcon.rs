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

use dpcore::canvas::CanvasState;
use dpcore::paint::*;
use dpcore::protocol::message::{CommandMessage, Message};

#[test]
fn test_simple_conflict() {
    let mut canvas = CanvasState::new();
    let white = Color::rgb8(255, 255, 255);

    // Create a small canvas with one layer filled with solid white
    canvas.receive_message(&m("1 resize right=64 bottom=64"));
    canvas.receive_message(&m("1 newlayer id=0x0101 fill=#ffffff"));
    assert_eq!(lc(&canvas), Some(white));

    // Now, let's draw something
    canvas.receive_local_message(&m("1 undopoint"));
    canvas.receive_local_message(&m(
        "1 fillrect layer=0x0101 x=0 y=0 w=10 h=10 color=#ff0000 mode=1",
    ));

    assert_eq!(lc(&canvas), None); // not solid white anymore

    // Second user draws a white patch over our patch: retcon removes our changes
    canvas.receive_message(&m(
        "2 fillrect layer=0x0101 x=5 y=5 w=10 h=10 color=#ffffff mode=1",
    ));

    assert_eq!(lc(&canvas), Some(white), "expected retcon"); // is solid white again
}

fn m(msg: &str) -> CommandMessage {
    match Message::from_text(&msg.parse().unwrap()).unwrap() {
        Message::Command(m) => m,
        _ => panic!("Not a command message: {msg}"),
    }
}

fn lc(canvas: &CanvasState) -> Option<Color> {
    canvas
        .layerstack()
        .root()
        .get_bitmaplayer(0x0101)
        .unwrap()
        .solid_color()
}
