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
use dpcore::paint::{AoE, FlattenedTileIterator, Rectangle};
use dpcore::protocol::message::Message;

use core::ffi::c_void;
use std::slice;

pub struct PaintEngine {
    canvas: CanvasState,
    changes: AoE,
}

#[no_mangle]
pub extern "C" fn paintengine_new() -> *mut PaintEngine {
    let dp = Box::new(PaintEngine {
        canvas: CanvasState::new(),
        changes: AoE::Nothing,
    });

    Box::into_raw(dp)
}

#[repr(C)]
pub struct Size {
    width: u32,
    height: u32,
}

#[no_mangle]
pub extern "C" fn paintengine_free(dp: *mut PaintEngine) {
    if !dp.is_null() {
        let dp = unsafe { Box::from_raw(dp) };
        drop(dp);
    }
}

#[no_mangle]
pub extern "C" fn paintengine_canvas_size(dp: &PaintEngine) -> Size {
    let ls = dp.canvas.layerstack();
    return Size {
        width: ls.width(),
        height: ls.height(),
    };
}

/// Receive one or more messages
/// Only Command type messages are handled.
#[no_mangle]
pub extern "C" fn paintengine_receive_messages(
    dp: &mut PaintEngine,
    messages: *const u8,
    messages_len: usize,
) {
    let msgs = unsafe { slice::from_raw_parts(messages, messages_len) };
    let mut aoe = AoE::Nothing;

    println!("Receiving messages {}", messages_len);

    let msg = Message::deserialize(msgs);
    match msg {
        Result::Ok(Message::Command(m)) => {
            aoe = aoe.merge(dp.canvas.receive_message(&m));
            println!("AoE: {:?}", aoe);
        }
        Result::Ok(_) => {
            // other messages are ignored
        }
        Result::Err(_e) => {
            // TODO
            panic!();
        }
    };

    if !aoe.is_nothing() {
        //let bounds = aoe.bounds();
        let changes = std::mem::replace(&mut dp.changes, AoE::Nothing);
        dp.changes = changes.merge(aoe);
        //dp.notify_changes(bounds);
    }
}

/// Paint all the changed tiles in the given area
///
/// For each changed tile in the area, the tile is flattened and the paint callback called.
/// The change flag is then cleared for that tile.
#[no_mangle]
pub extern "C" fn paintengine_paint_changes(
    dp: &PaintEngine,
    ctx: *mut c_void,
    rect: Rectangle,
    paint_func: extern "C" fn(ctx: *mut c_void, x: i32, y: i32, pixels: *const u8),
) {
    let area = if rect.w < 0 {
        AoE::Everything
    } else {
        let rect = rect.cropped(
            dp.canvas.layerstack().width(),
            dp.canvas.layerstack().height(),
        );
        match rect {
            Some(r) => AoE::Bounds(r),
            None => {
                return;
            }
        }
    };

    // TODO intersect with requested area with the actual changes:
    // changes_cleared, intersected = intersect(changes, area)
    FlattenedTileIterator::new(&dp.canvas.layerstack(), area)
        .for_each(|(x, y, t)| paint_func(ctx, x, y, t.pixels.as_ptr() as *const u8));
}

impl PaintEngine {
    fn notify_change(&mut self, bounds: Rectangle) {}
}
