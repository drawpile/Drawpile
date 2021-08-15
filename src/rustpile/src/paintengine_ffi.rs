// This file is part of Drawpile.
// Copyright (C) 2021 Calle Laakkonen
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
use dpcore::paint::{AoE, FlattenedTileIterator, Rectangle, Size};
use dpcore::protocol::message::Message;

use core::ffi::c_void;
use std::ptr;
use std::slice;

type NotifyChangesCallback = Option<extern "C" fn(ctx: *mut c_void, area: Rectangle)>;
type NotifyResizeCallback =
    Option<extern "C" fn(ctx: *mut c_void, x_offset: i32, y_offset: i32, old_size: Size)>;

/// The paint engine.
pub struct PaintEngine {
    canvas: CanvasState,
    changes: AoE,

    change_context_object: *mut c_void,
    notify_changes_callback: NotifyChangesCallback,
    notify_resize_callback: NotifyResizeCallback,
}

/// Construct a new paint engine with an empty canvas.
#[no_mangle]
pub extern "C" fn paintengine_new() -> *mut PaintEngine {
    let dp = Box::new(PaintEngine {
        canvas: CanvasState::new(),
        changes: AoE::Nothing,
        change_context_object: ptr::null_mut(),
        notify_changes_callback: None,
        notify_resize_callback: None,
    });

    Box::into_raw(dp)
}

/// Delete a paint engine instance
#[no_mangle]
pub extern "C" fn paintengine_free(dp: *mut PaintEngine) {
    if !dp.is_null() {
        let dp = unsafe { Box::from_raw(dp) };
        drop(dp);
    }
}

/// Register callback functions for notifying canvas view of changes
///
/// The paintengine can only be observed by one view at a time.
#[no_mangle]
pub extern "C" fn paintengine_register_notify_callbacks(
    dp: &mut PaintEngine,
    ctx: *mut c_void,
    changes: NotifyChangesCallback,
    resizes: NotifyResizeCallback,
) {
    dp.change_context_object = ctx;
    dp.notify_changes_callback = changes;
    dp.notify_resize_callback = resizes;
}

/// Get the current size of the canvas.
#[no_mangle]
pub extern "C" fn paintengine_canvas_size(dp: &PaintEngine) -> Size {
    dp.canvas.layerstack().size()
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

    println!("Receiving {} messages", messages_len);

    // TODO loop and consume all messages, or specify that this function
    // only takes one.

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

    // Notify view layer of changed regions
    // This doesn't immediately trigger a repaint if no view is observing the changed
    // region. Repainting is lazy: changed tiles are not refreshed until they are actually
    // being viewed.
    match aoe {
        AoE::Nothing => {}
        AoE::Resize(rx, ry, original_size) => {
            dp.changes = AoE::Everything;
            dp.notify_canvas_resize(
                rx,
                ry,
                original_size
            );
        }
        _ => {
            let canvas_size = paintengine_canvas_size(dp);
            let aoe_bounds = aoe.bounds(canvas_size);
            match aoe_bounds {
                Some(bounds) => {
                    let changes = std::mem::replace(&mut dp.changes, AoE::Nothing);
                    dp.changes = changes.merge(aoe);
                    dp.notify_changes(bounds);
                }
                None => {}
            }
        }
    }
}

/// Paint all the changed tiles in the given area
///
/// A paintengine instance can only have a single observer (which itself can be
/// a caching layer that is observed by multiple views,) so it keeps track of which
/// tiles have changed since they were last painted. Calling this function will flatten
/// all tiles intersecting the given region of interest and call the provided paint fallback
/// function for each tile with the raw flattened pixel data. The dirty flag is then
/// cleared for the repainted tile.
#[no_mangle]
pub extern "C" fn paintengine_paint_changes(
    dp: &mut PaintEngine,
    ctx: *mut c_void,
    rect: Rectangle,
    paint_func: extern "C" fn(ctx: *mut c_void, x: i32, y: i32, pixels: *const u8),
) {
    let intersection = if rect.w < 0 {
        // We interpret an invalid rectangle to mean "refresh everything"
        std::mem::replace(&mut dp.changes, AoE::Nothing)
    } else {
        let ls = dp.canvas.layerstack();
        let rect = match rect.cropped(ls.size()) {
            Some(r) => r,
            None => {
                return;
            }
        };

        let changes = std::mem::replace(&mut dp.changes, AoE::Nothing);
        let (difference, intersection) =
            changes.diff_and_intersect(rect, ls.size());
        dp.changes = difference;

        intersection
    };

    FlattenedTileIterator::new(&dp.canvas.layerstack(), intersection)
        .for_each(|(x, y, t)| paint_func(ctx, x, y, t.pixels.as_ptr() as *const u8));
}

impl PaintEngine {
    fn notify_changes(&self, bounds: Rectangle) {
        match self.notify_changes_callback {
            Some(f) => f(self.change_context_object, bounds),
            None => {}
        }
    }

    fn notify_canvas_resize(&self, xoffset: i32, yoffset: i32, old_size: Size) {
        println!("Notifying of resize {},{} {:?}", xoffset, yoffset, old_size);
        match self.notify_resize_callback {
            Some(f) => f(self.change_context_object, xoffset, yoffset, old_size),
            None => {}
        }
    }
}
