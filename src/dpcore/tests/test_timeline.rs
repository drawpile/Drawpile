// This file is part of Drawpile.
// Copyright (C) 2022 Calle Laakkonen
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
use dpcore::paint::color::ZERO_PIXEL8;
use dpcore::paint::*;
use dpcore::protocol::message::{CommandMessage, Message};

const BLACK_PIXEL8: Pixel8 = [0, 0, 0, 255];

#[test]
fn test_timeline() {
    let mut canvas = CanvasState::new();

    // Initialize a 4x4 tile canvas with black tiles in each corner.
    // layer 1 - top left
    // layer 2 - top right
    // layer 3 - bottom right
    // layer 4 - bottom left

    canvas.receive_message(&m("1 resize right=128 bottom=128"));
    canvas.receive_message(&m("1 newlayer id=0x0101"));
    canvas.receive_message(&m("1 newlayer id=0x0102"));
    canvas.receive_message(&m("1 newlayer id=0x0103"));
    canvas.receive_message(&m("1 newlayer id=0x0104"));

    canvas.receive_message(&m(
        "1 fillrect layer=0x0101 x=0 y=0 w=64 h=64 color=#000000 mode=1",
    ));
    canvas.receive_message(&m(
        "1 fillrect layer=0x0102 x=64 y=0 w=64 h=64 color=#000000 mode=1",
    ));
    canvas.receive_message(&m(
        "1 fillrect layer=0x0103 x=64 y=64 w=64 h=64 color=#000000 mode=1",
    ));
    canvas.receive_message(&m(
        "1 fillrect layer=0x0104 x=0 y=64 w=64 h=64 color=#000000 mode=1",
    ));

    let t1 = (32, 32);
    let t2 = (96, 32);
    let t3 = (96, 96);
    let t4 = (32, 96);

    // The canvas starts out in auto-timeline mode where each
    // frame corresponds to a top-level layer
    assert_eq!(color_at(&canvas, t1, 0), BLACK_PIXEL8);
    assert_eq!(color_at(&canvas, t2, 0), ZERO_PIXEL8);
    assert_eq!(color_at(&canvas, t3, 0), ZERO_PIXEL8);
    assert_eq!(color_at(&canvas, t4, 0), ZERO_PIXEL8);

    assert_eq!(color_at(&canvas, t1, 1), ZERO_PIXEL8);
    assert_eq!(color_at(&canvas, t2, 1), BLACK_PIXEL8);
    assert_eq!(color_at(&canvas, t3, 1), ZERO_PIXEL8);
    assert_eq!(color_at(&canvas, t4, 1), ZERO_PIXEL8);

    assert_eq!(color_at(&canvas, t1, 2), ZERO_PIXEL8);
    assert_eq!(color_at(&canvas, t2, 2), ZERO_PIXEL8);
    assert_eq!(color_at(&canvas, t3, 2), BLACK_PIXEL8);
    assert_eq!(color_at(&canvas, t4, 2), ZERO_PIXEL8);

    assert_eq!(color_at(&canvas, t1, 3), ZERO_PIXEL8);
    assert_eq!(color_at(&canvas, t2, 3), ZERO_PIXEL8);
    assert_eq!(color_at(&canvas, t3, 3), ZERO_PIXEL8);
    assert_eq!(color_at(&canvas, t4, 3), BLACK_PIXEL8);

    // Now, let's active the custom timeline
    // The frames will be:
    // 0: 1, 4
    // 1: 2, 4
    // 2: 3, 4
    // 3: 4
    // 4: 1
    canvas.receive_message(&m("1 setmetadataint field=3 value=1"));
    canvas.receive_message(&m(
        "1 settimelineframe frame=0 layers=0x0101,0x0104,0,0,0,0,0,0,0,0,0,0",
    ));
    canvas.receive_message(&m(
        "1 settimelineframe frame=1 layers=0x0102,0x0104,0,0,0,0,0,0,0,0,0,0",
    ));
    canvas.receive_message(&m(
        "1 settimelineframe frame=2 layers=0x0103,0x0104,0,0,0,0,0,0,0,0,0,0",
    ));
    canvas.receive_message(&m(
        "1 settimelineframe frame=3 layers=0x0104,0,0,0,0,0,0,0,0,0,0,0",
    ));
    canvas.receive_message(&m(
        "1 settimelineframe frame=4 layers=0x0101,0,0,0,0,0,0,0,0,0,0,0",
    ));

    assert_eq!(color_at(&canvas, t1, 0), BLACK_PIXEL8);
    assert_eq!(color_at(&canvas, t2, 0), ZERO_PIXEL8);
    assert_eq!(color_at(&canvas, t3, 0), ZERO_PIXEL8);
    assert_eq!(color_at(&canvas, t4, 0), BLACK_PIXEL8);

    assert_eq!(color_at(&canvas, t1, 1), ZERO_PIXEL8);
    assert_eq!(color_at(&canvas, t2, 1), BLACK_PIXEL8);
    assert_eq!(color_at(&canvas, t3, 1), ZERO_PIXEL8);
    assert_eq!(color_at(&canvas, t4, 1), BLACK_PIXEL8);

    assert_eq!(color_at(&canvas, t1, 2), ZERO_PIXEL8);
    assert_eq!(color_at(&canvas, t2, 2), ZERO_PIXEL8);
    assert_eq!(color_at(&canvas, t3, 2), BLACK_PIXEL8);
    assert_eq!(color_at(&canvas, t4, 2), BLACK_PIXEL8);

    assert_eq!(color_at(&canvas, t1, 3), ZERO_PIXEL8);
    assert_eq!(color_at(&canvas, t2, 3), ZERO_PIXEL8);
    assert_eq!(color_at(&canvas, t3, 3), ZERO_PIXEL8);
    assert_eq!(color_at(&canvas, t4, 3), BLACK_PIXEL8);

    assert_eq!(color_at(&canvas, t1, 4), BLACK_PIXEL8);
    assert_eq!(color_at(&canvas, t2, 4), ZERO_PIXEL8);
    assert_eq!(color_at(&canvas, t3, 4), ZERO_PIXEL8);
    assert_eq!(color_at(&canvas, t4, 4), ZERO_PIXEL8);

    // Frames outside the assigned range are blank
    assert_eq!(color_at(&canvas, t1, 5), ZERO_PIXEL8);
    assert_eq!(color_at(&canvas, t2, 5), ZERO_PIXEL8);
    assert_eq!(color_at(&canvas, t3, 5), ZERO_PIXEL8);
    assert_eq!(color_at(&canvas, t4, 5), ZERO_PIXEL8);

    // Let's try deleting the first frame
    canvas.receive_message(&m("1 removetimelineframe frame=0"));

    assert_eq!(color_at(&canvas, t1, 0), ZERO_PIXEL8);
    assert_eq!(color_at(&canvas, t2, 0), BLACK_PIXEL8);
    assert_eq!(color_at(&canvas, t3, 0), ZERO_PIXEL8);
    assert_eq!(color_at(&canvas, t4, 0), BLACK_PIXEL8);

    // Then insert a new one in its place
    canvas.receive_message(&m(
        "1 settimelineframe frame=0 insert=true layers=0x0101,0,0,0,0,0,0,0,0,0,0,0",
    ));

    assert_eq!(color_at(&canvas, t1, 0), BLACK_PIXEL8);
    assert_eq!(color_at(&canvas, t2, 0), ZERO_PIXEL8);
    assert_eq!(color_at(&canvas, t3, 0), ZERO_PIXEL8);
    assert_eq!(color_at(&canvas, t4, 0), ZERO_PIXEL8);

    assert_eq!(color_at(&canvas, t1, 1), ZERO_PIXEL8);
    assert_eq!(color_at(&canvas, t2, 1), BLACK_PIXEL8);
    assert_eq!(color_at(&canvas, t3, 1), ZERO_PIXEL8);
    assert_eq!(color_at(&canvas, t4, 1), BLACK_PIXEL8);
}

#[test]
fn test_timeline_groups() {
    let mut canvas = CanvasState::new();

    // Initialize a 1x1 tile canvas with the following layers:
    // 1. Regular layer: black pixel at 1,1
    // 2. non-isolated group
    // 2.1. layer with black pixel at 2,1
    // 2.2. layer with black pixel at 2,2
    // 3. regular group
    // 3.1 layer with black pixel at 3,1
    // 4. Regular layer: black pixel at 4,1

    // the timeline will consists of the following frames:
    // 0. layer 1
    // 1. layer 2.1
    // 2. layer 2.2
    // 3. layer 3
    // 4. layer 4
    canvas.receive_message(&m("1 resize right=64 bottom=64"));
    canvas.receive_message(&m("1 newlayer id=0x0101"));
    canvas.receive_message(&m("1 newlayer id=0x0102 flags=group"));
    canvas.receive_message(&m("1 layerattr id=0x0102 opacity=255 blend=1")); // remove "isolated" flag
    canvas.receive_message(&m("1 newlayer id=0x0201 target=0x0102 flags=into"));
    canvas.receive_message(&m("1 newlayer id=0x0202 target=0x0102 flags=into"));
    canvas.receive_message(&m("1 newlayer id=0x0103 flags=group"));
    canvas.receive_message(&m("1 newlayer id=0x0301 target=0x0103 flags=into"));
    canvas.receive_message(&m("1 newlayer id=0x0104"));

    canvas.receive_message(&m(
        "1 fillrect layer=0x0101 x=1 y=1 w=1 h=1 color=#000000 mode=1",
    ));
    canvas.receive_message(&m(
        "1 fillrect layer=0x0201 x=1 y=2 w=1 h=1 color=#000000 mode=1",
    ));
    canvas.receive_message(&m(
        "1 fillrect layer=0x0202 x=2 y=2 w=1 h=1 color=#000000 mode=1",
    ));
    canvas.receive_message(&m(
        "1 fillrect layer=0x0301 x=1 y=3 w=1 h=1 color=#000000 mode=1",
    ));
    canvas.receive_message(&m(
        "1 fillrect layer=0x0104 x=1 y=4 w=1 h=1 color=#000000 mode=1",
    ));

    canvas.receive_message(&m(
        "1 settimelineframe frame=0 layers=0x0101,0,0,0,0,0,0,0,0,0,0,0",
    ));
    canvas.receive_message(&m(
        "1 settimelineframe frame=1 layers=0x0201,0,0,0,0,0,0,0,0,0,0,0",
    ));
    canvas.receive_message(&m(
        "1 settimelineframe frame=2 layers=0x0202,0,0,0,0,0,0,0,0,0,0,0",
    ));
    canvas.receive_message(&m(
        "1 settimelineframe frame=3 layers=0x0103,0,0,0,0,0,0,0,0,0,0,0",
    ));
    canvas.receive_message(&m(
        "1 settimelineframe frame=4 layers=0x0104,0,0,0,0,0,0,0,0,0,0,0",
    ));

    // Custom timeline not yet enabled, test the autogenerated one.
    // TODO

    // Enable custom timeline
    canvas.receive_message(&m("1 setmetadataint field=3 value=1"));
    assert_eq!(color_at(&canvas, (1, 1), 0), BLACK_PIXEL8);
    assert_eq!(color_at(&canvas, (1, 2), 0), ZERO_PIXEL8);
    assert_eq!(color_at(&canvas, (2, 2), 0), ZERO_PIXEL8);
    assert_eq!(color_at(&canvas, (1, 3), 0), ZERO_PIXEL8);
    assert_eq!(color_at(&canvas, (1, 4), 0), ZERO_PIXEL8);

    assert_eq!(color_at(&canvas, (1, 1), 1), ZERO_PIXEL8);
    assert_eq!(color_at(&canvas, (1, 2), 1), BLACK_PIXEL8);
    assert_eq!(color_at(&canvas, (2, 2), 1), ZERO_PIXEL8);
    assert_eq!(color_at(&canvas, (1, 3), 1), ZERO_PIXEL8);
    assert_eq!(color_at(&canvas, (1, 4), 1), ZERO_PIXEL8);

    assert_eq!(color_at(&canvas, (1, 1), 2), ZERO_PIXEL8);
    assert_eq!(color_at(&canvas, (1, 2), 2), ZERO_PIXEL8);
    assert_eq!(color_at(&canvas, (2, 2), 2), BLACK_PIXEL8);
    assert_eq!(color_at(&canvas, (1, 3), 2), ZERO_PIXEL8);
    assert_eq!(color_at(&canvas, (1, 4), 2), ZERO_PIXEL8);

    assert_eq!(color_at(&canvas, (1, 1), 3), ZERO_PIXEL8);
    assert_eq!(color_at(&canvas, (1, 2), 3), ZERO_PIXEL8);
    assert_eq!(color_at(&canvas, (2, 2), 3), ZERO_PIXEL8);
    assert_eq!(color_at(&canvas, (1, 3), 3), BLACK_PIXEL8);
    assert_eq!(color_at(&canvas, (1, 4), 3), ZERO_PIXEL8);

    assert_eq!(color_at(&canvas, (1, 1), 4), ZERO_PIXEL8);
    assert_eq!(color_at(&canvas, (1, 2), 4), ZERO_PIXEL8);
    assert_eq!(color_at(&canvas, (2, 2), 4), ZERO_PIXEL8);
    assert_eq!(color_at(&canvas, (1, 3), 4), ZERO_PIXEL8);
    assert_eq!(color_at(&canvas, (1, 4), 4), BLACK_PIXEL8);
}

fn m(msg: &str) -> CommandMessage {
    match Message::from_text(&msg.parse().unwrap()).unwrap() {
        Message::Command(m) => m,
        _ => panic!("Not a command message: {msg}"),
    }
}

fn color_at(canvas: &CanvasState, pos: (usize, usize), frame: isize) -> Pixel8 {
    let img = canvas.layerstack().to_image(&LayerViewOptions::frame(
        canvas.layerstack().frame_at(frame),
    ));
    let p = img.pixels[img.width * pos.1 + pos.0];
    println!(
        "Color at {:?} frame {} contains {:?} = {:?}",
        pos,
        frame,
        canvas.layerstack().frame_at(frame),
        p
    );
    p
}
