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

use super::adapters::LayerInfo;
use dpcore::canvas::{CanvasState, CanvasStateChange};
use dpcore::paint::{LayerStack, AoE, FlattenedTileIterator, Rectangle, Size};
use dpcore::protocol::message::{Message, CommandMessage};

use core::ffi::c_void;
use std::{slice, thread};
use std::sync::{Arc, Mutex, mpsc};
use std::sync::mpsc::{Sender, Receiver};

use tracing::{error, warn};

type NotifyChangesCallback = extern "C" fn(ctx: *mut c_void, area: Rectangle);
type NotifyResizeCallback = extern "C" fn(ctx: *mut c_void, x_offset: i32, y_offset: i32, old_size: Size);
type NotifyLayerListCallback = extern "C" fn(ctx: *mut c_void, layers: *const LayerInfo, count: usize);

/// A copy of the layerstack to use in the main thread while the
/// paint engine is busy
struct ViewCache {
    layerstack: Arc<LayerStack>,
    unrefreshed_area: AoE,
}

/// The callbacks used to (asynchronously) notify the view layer of
/// changes to the canvas.
/// Note that these callbacks are called in the *paint engine thread*.
/// They should only be used to post events to the main thread's event loop.
struct NotificationCallbacks {
    context_object: *mut c_void,
    notify_changes: NotifyChangesCallback,
    notify_resize: NotifyResizeCallback,
    notify_layerlist: NotifyLayerListCallback,
}

// Unsafe due to the c_void pointer
unsafe impl Send for NotificationCallbacks {}

/// Commands sent to the paint engine
enum PaintEngineCommand {
    LocalMessage(CommandMessage),
    RemoteMessage(CommandMessage),
}

/// The paint engine
pub struct PaintEngine {
    /// A copy of the canvas state for read-only use in the view layer
    viewcache: Arc<Mutex<ViewCache>>,

    /// A channel for sending commands to the paint engine
    engine_channel: Sender<PaintEngineCommand>,

    /// The paint engine thread's handle
    thread_handle: thread::JoinHandle<()>,
}

fn run_paintengine(viewcache: Arc<Mutex<ViewCache>>, channel: Receiver<PaintEngineCommand>, callbacks: NotificationCallbacks) {
    let mut canvas = CanvasState::new();

    loop {
        // Wait for a command
        let mut cmd = match channel.recv() {
            Ok(m) => m,
            Err(_) => { return; },
        };

        let mut changes = CanvasStateChange::nothing();

        // Execute the received command and see if there are more queued
        loop {
            use PaintEngineCommand::*;
            match cmd {
                LocalMessage(m) => { changes |= canvas.receive_local_message(&m); }
                RemoteMessage(m) => { changes |= canvas.receive_message(&m); }
            };

            cmd = match channel.try_recv() {
                Ok(m) => m,
                Err(mpsc::TryRecvError::Empty) => { break; },
                Err(mpsc::TryRecvError::Disconnected) => { return; },
            };
        }

        // Notify changes
        if changes.has_any() {
            {
                let mut vc = viewcache.lock().unwrap();
                vc.layerstack = canvas.arc_layerstack();

                match changes.aoe {
                    AoE::Nothing => {}
                    AoE::Resize(rx, ry, original_size) => {
                        vc.unrefreshed_area = AoE::Everything;
                        (callbacks.notify_resize)(callbacks.context_object, rx, ry, original_size);
                    }
                    _ => {
                        let canvas_size = canvas.layerstack().size();
                        if let Some(bounds) = changes.aoe.bounds(canvas_size) {
                            let unrefreshed_area = std::mem::replace(&mut vc.unrefreshed_area, AoE::Nothing);
                            vc.unrefreshed_area = unrefreshed_area.merge(changes.aoe);
                            (callbacks.notify_changes)(callbacks.context_object, bounds);
                        }
                    }
                }
            }

            if changes.layers_changed {
                let layers: Vec<LayerInfo> = canvas
                    .layerstack()
                    .iter_layers()
                    .map(|l| l.into())
                    .collect();
                (callbacks.notify_layerlist)(callbacks.context_object, layers.as_ptr(), layers.len());
            }

            if changes.annotations_changed {}
        }
    }
}

/// Construct a new paint engine with an empty canvas.
#[no_mangle]
pub extern "C" fn paintengine_new(
    ctx: *mut c_void,
    changes: NotifyChangesCallback,
    resizes: NotifyResizeCallback,
    layers: NotifyLayerListCallback,
) -> *mut PaintEngine {
    let viewcache = Arc::new(Mutex::new(ViewCache{
        layerstack: Arc::new(LayerStack::new(0, 0)),
        unrefreshed_area: AoE::Nothing,
    }));

    let callbacks = NotificationCallbacks {
        context_object: ctx,
        notify_changes: changes,
        notify_resize: resizes,
        notify_layerlist: layers,
    };

    let (sender, receiver) = mpsc::channel::<PaintEngineCommand>();

    let dp = Box::new(PaintEngine {
        viewcache: viewcache.clone(),
        engine_channel: sender,
        thread_handle: thread::spawn(move || {
            run_paintengine(viewcache, receiver, callbacks);
        }),
    });

    Box::into_raw(dp)
}

/// Delete a paint engine instance and wait for its thread to finish
#[no_mangle]
pub extern "C" fn paintengine_free(dp: *mut PaintEngine) {
    if !dp.is_null() {
        let handle = {
            let dp = unsafe { Box::from_raw(dp) };
            dp.thread_handle
        };

        if let Err(err) = handle.join() {
            error!("Paintengine thread exited with an error {:?}", err);
        }
    }
}

/// Get the current size of the canvas.
#[no_mangle]
pub extern "C" fn paintengine_canvas_size(dp: &PaintEngine) -> Size {
    let vc = dp.viewcache.lock().unwrap();
    vc.layerstack.size()
}

/// Receive one or more messages
/// Only Command type messages are handled.
#[no_mangle]
pub extern "C" fn paintengine_receive_messages(
    dp: &mut PaintEngine,
    local: bool,
    messages: *const u8,
    messages_len: usize,
) {
    let msgs = unsafe { slice::from_raw_parts(messages, messages_len) };

    // TODO loop and consume all messages, or specify that this function
    // only takes one.

    let msg = Message::deserialize(msgs);
    match msg {
        Result::Ok(Message::Command(m)) => {
            if let Err(err) = dp.engine_channel.send(
                if local {
                    PaintEngineCommand::LocalMessage(m)
                } else {
                    PaintEngineCommand::RemoteMessage(m)
                }
            ) {
                warn!("Couldn't send command to paint engine thread {:?}", err);
            }
        }
        Result::Ok(_) => {
            // other messages are ignored
        }
        Result::Err(_e) => {
            // TODO
            panic!();
        }
    };
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
    let mut vc = dp.viewcache.lock().unwrap();

    let intersection = if rect.w < 0 {
        // We interpret an invalid rectangle to mean "refresh everything"
        std::mem::replace(&mut vc.unrefreshed_area, AoE::Nothing)
    } else {
        let ls = vc.layerstack.size();
        let rect = match rect.cropped(ls) {
            Some(r) => r,
            None => {
                return;
            }
        };

        let unrefreshed_area = std::mem::replace(&mut vc.unrefreshed_area, AoE::Nothing);
        let (remaining, intersection) = unrefreshed_area.diff_and_intersect(rect, ls);
        vc.unrefreshed_area = remaining;

        intersection
    };

    FlattenedTileIterator::new(&vc.layerstack, intersection)
        .for_each(|(x, y, t)| paint_func(ctx, x, y, t.pixels.as_ptr() as *const u8));
}
