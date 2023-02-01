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
use dpcore::protocol::message::{CommandMessage, Message, UNDO_DEPTH};

#[test]
fn test_simple_undo() {
    let mut canvas = CanvasState::new();
    let white = Color::rgb8(255, 255, 255);
    let red = Color::rgb8(255, 0, 0);
    let green = Color::rgb8(0, 255, 0);

    // Create a small canvas with one layer filled with solid white
    canvas.receive_message(&m("1 resize right=64 bottom=64"));
    canvas.receive_message(&m("1 newlayer id=0x0101 fill=#ffffff"));

    assert_eq!(lc(&canvas), Some(white));

    // No first undopoint: this shouldn't do anything
    canvas.receive_message(&m("1 undo"));
    assert_eq!(
        lc(&canvas),
        Some(white),
        "undo without first undopoint shouldn't have done anything"
    );

    // First undoable sequence
    canvas.receive_message(&m("1 undopoint"));
    canvas.receive_message(&m("1 fillrect layer=0x0101 w=64 h=64 color=#ff0000 mode=1"));
    assert_eq!(lc(&canvas), Some(red));

    // Undoing it should return the canvas to white
    canvas.receive_message(&m("1 undo"));
    assert_eq!(
        lc(&canvas),
        Some(white),
        "undo should have returned the canvas to white"
    );

    // Redoing it should bring the red back
    canvas.receive_message(&m("1 undo redo=true"));
    assert_eq!(
        lc(&canvas),
        Some(red),
        "redo should have brought the red back"
    );

    // Let's undo it again and fill the canvas with green
    // The undon sequence that fills the canvas with red will be marked as gone
    canvas.receive_message(&m("1 undo"));
    assert_eq!(
        lc(&canvas),
        Some(white),
        "second undo should have removed the red"
    );
    canvas.receive_message(&m("1 undopoint"));
    canvas.receive_message(&m("1 fillrect layer=0x0101 w=64 h=64 color=#00ff00 mode=1"));
    assert_eq!(lc(&canvas), Some(green));

    // First undo goes back to white, not red
    canvas.receive_message(&m("1 undo"));
    assert_eq!(
        lc(&canvas),
        Some(white),
        "undo after an undopoint should have returned the canvas to white"
    );

    // Redo goes back to geen, not red
    canvas.receive_message(&m("1 undo redo=true"));
    assert_eq!(
        lc(&canvas),
        Some(green),
        "redo should have returned the green"
    );
}

#[test]
fn test_branching_undo() {
    let mut canvas = CanvasState::new();
    let white = Color::rgb8(255, 255, 255);
    let red = Color::rgb8(255, 0, 0);
    let green = Color::rgb8(0, 255, 0);
    let blue = Color::rgb8(0, 0, 255);

    // Create a small canvas with one layer filled with solid white
    canvas.receive_message(&m("1 resize right=64 bottom=64"));
    canvas.receive_message(&m("1 newlayer id=0x0101 fill=#ffffff"));

    assert_eq!(lc(&canvas), Some(white));

    // First undoable sequence (fill red)
    canvas.receive_message(&m("1 undopoint"));
    canvas.receive_message(&m("1 fillrect layer=0x0101 w=64 h=64 color=#ff0000 mode=1"));
    assert_eq!(lc(&canvas), Some(red));

    // Second undoable sequence (fill green)
    canvas.receive_message(&m("1 undopoint"));
    canvas.receive_message(&m("1 fillrect layer=0x0101 w=64 h=64 color=#00ff00 mode=1"));
    assert_eq!(lc(&canvas), Some(green));

    // Undoing #2 should return the canvas to red
    canvas.receive_message(&m("1 undo"));
    assert_eq!(
        lc(&canvas),
        Some(red),
        "undo should have returned the canvas to red"
    );

    // Third undoable sequence.
    // This branches the history and the sequence #2 should now be gone
    canvas.receive_message(&m("1 undopoint"));
    canvas.receive_message(&m("1 fillrect layer=0x0101 w=64 h=64 color=#0000ff mode=1"));
    assert_eq!(lc(&canvas), Some(blue));

    // Undoing #3 should return the canvas to red (again)
    canvas.receive_message(&m("1 undo"));
    assert_eq!(
        lc(&canvas),
        Some(red),
        "undo should have returned the canvas to red again"
    );

    // Undoing #1 should return the canvas to white
    canvas.receive_message(&m("1 undo"));
    assert_eq!(
        lc(&canvas),
        Some(white),
        "undo should have returned the canvas to red again"
    );

    // A redo now should bring the red back
    canvas.receive_message(&m("1 undo redo=true"));
    assert_eq!(lc(&canvas), Some(red), "redo should have returned the red");

    // And then the blue
    canvas.receive_message(&m("1 undo redo=true"));
    assert_eq!(
        lc(&canvas),
        Some(blue),
        "redo should have returned the blue"
    );
}

#[test]
fn test_multiuser_undo() {
    let mut canvas = CanvasState::new();
    let white = Color::rgb8(255, 255, 255);
    let red = Color::rgb8(255, 0, 0);
    let green = Color::rgb8(0, 255, 0);

    // Create a small canvas with one layer filled with solid white
    canvas.receive_message(&m("1 resize right=64 bottom=64"));
    canvas.receive_message(&m("1 newlayer id=0x0101 fill=#ffffff"));

    // User 1 fills the canvas with red
    canvas.receive_message(&m("1 undopoint"));
    canvas.receive_message(&m("1 fillrect layer=0x0101 w=64 h=64 color=#ff0000 mode=1"));
    assert_eq!(lc(&canvas), Some(red), "should be filled with red now");

    // User 2 fills the canvas with green
    canvas.receive_message(&m("2 undopoint"));
    canvas.receive_message(&m("2 fillrect layer=0x0101 w=64 h=64 color=#00ff00 mode=1"));
    assert_eq!(lc(&canvas), Some(green), "should be filled with green now");

    // User 1 undoes their fill: canvas is still green
    canvas.receive_message(&m("1 undo"));
    assert_eq!(
        lc(&canvas),
        Some(green),
        "red fill undone, but green fill should have been redone"
    );

    // User 2 undoes their fill: canvas is white again
    canvas.receive_message(&m("2 undo"));
    assert_eq!(lc(&canvas), Some(white), "both fills undone");
}

#[test]
fn test_undo_depth() {
    let mut canvas = CanvasState::new();
    let red = Color::rgb8(255, 0, 0);
    let green = Color::rgb8(0, 255, 0);

    // Create a small canvas with one layer filled with solid white
    canvas.receive_message(&m("1 resize right=64 bottom=64"));
    canvas.receive_message(&m("1 newlayer id=0x0101 fill=#ffffff"));

    // First undoable sequence (fill red)
    canvas.receive_message(&m("1 undopoint"));
    canvas.receive_message(&m("1 fillrect layer=0x0101 w=64 h=64 color=#ff0000 mode=1"));
    assert_eq!(lc(&canvas), Some(red));

    // Now fill up the undo stack
    for _ in 0..UNDO_DEPTH {
        canvas.receive_message(&m("1 undopoint"));
    }

    // Final undoable sequence (fill with green)
    canvas.receive_message(&m("1 undopoint"));
    canvas.receive_message(&m("1 fillrect layer=0x0101 w=64 h=64 color=#00ff00 mode=1"));
    assert_eq!(lc(&canvas), Some(green));

    // Go back to red
    canvas.receive_message(&m("1 undo"));
    assert_eq!(lc(&canvas), Some(red), "failed to undo");

    // Now, this undo shouldn't work anymore
    canvas.receive_message(&m("1 undo"));
    assert_eq!(lc(&canvas), Some(red), "undo stack didn't fill up?");
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
