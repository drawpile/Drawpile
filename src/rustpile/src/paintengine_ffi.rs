// This file is part of Drawpile.
// Copyright (C) 2021-2022 Calle Laakkonen
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

use super::adapters::{flatten_layerinfo, AnnotationAt, Annotations, LayerInfo};
use super::messages_ffi;
use super::recindex::{build_index, IndexBuildProgressNoticationFn, IndexReader};
use super::snapshots::SnapshotQueue;
use dpcore::brush::{BrushEngine, BrushState};
use dpcore::canvas::images::make_putimage;
use dpcore::canvas::snapshot::make_canvas_snapshot;
use dpcore::canvas::{CanvasState, CanvasStateChange};
use dpcore::paint::annotation::AnnotationID;
use dpcore::paint::color::{pixels15_to_8, ZERO_PIXEL8};
use dpcore::paint::editstack;
use dpcore::paint::floodfill;
use dpcore::paint::tile::{TILE_LENGTH, TILE_SIZEI};
use dpcore::paint::{
    AoE, Blendmode, Color, FlattenedTileIterator, Frame, Image8, LayerID, LayerInsertion,
    LayerStack, LayerViewMode, LayerViewOptions, Pixel8, Rectangle, Size, Tile, UserID,
};
use dpcore::protocol::aclfilter::*;
use dpcore::protocol::message::*;
use dpcore::protocol::{
    DeserializationError, MessageReader, MessageWriter, DRAWPILE_VERSION, PROTOCOL_VERSION,
};

use dpimpex;
use dpimpex::rec::reader as rec_reader;
use dpimpex::rec::writer as rec_writer;

use core::ffi::c_void;
use std::collections::HashMap;
use std::convert::TryFrom;
use std::fs::File;
use std::path::{Path, PathBuf};
use std::sync::mpsc::{Receiver, Sender};
use std::sync::{mpsc, Arc, Mutex};
use std::time::{Duration, Instant};
use std::{ptr, slice, thread};

use tracing::{error, info, warn};

// Paint thread callbacks:
type NotifyChangesCallback = extern "C" fn(ctx: *mut c_void, area: Rectangle);
type NotifyResizeCallback =
    extern "C" fn(ctx: *mut c_void, x_offset: i32, y_offset: i32, old_size: Size);
type NotifyLayerListCallback =
    extern "C" fn(ctx: *mut c_void, layers: *const LayerInfo, count: usize);
type NotifyAnnotationsCallback = extern "C" fn(ctx: *mut c_void, annotations: *mut Annotations);
type NotifyCursorCallback =
    extern "C" fn(ctx: *mut c_void, user: UserID, layer: u16, x: i32, y: i32);
type NotifyPlaybackCallback = extern "C" fn(ctx: *mut c_void, pos: i64, interval: u32);
type NotifyCatchupCallback = extern "C" fn(ctx: *mut c_void, progress: u32);
type NotifyMetadataCallback = extern "C" fn(ctx: *mut c_void);
type NotifyTimelineCallback = extern "C" fn(ctx: *mut c_void);
type NotifyFrameVisibilityCallback =
    extern "C" fn(ctx: *mut c_void, Frame: *const Frame, frame_mode: bool);

// Main thread callbacks:
type JoinCallback = Option<
    extern "C" fn(
        ctx: *mut c_void,
        user: UserID,
        flags: u8,
        name: *const u8,
        name_len: usize,
        avatar: *const u8,
        avatar_len: usize,
    ),
>;
type LeaveCallback = Option<extern "C" fn(ctx: *mut c_void, user: UserID)>;
type ChatCallback = Option<
    extern "C" fn(
        ctx: *mut c_void,
        sender: UserID,
        recipient: UserID,
        tflags: u8,
        oflags: u8,
        message: *const u8,
        message_len: usize,
    ),
>;
type LaserCallback =
    Option<extern "C" fn(ctx: *mut c_void, user: UserID, persistence: u8, color: u32)>;
type DefaultLayerCallback = Option<extern "C" fn(ctx: *mut c_void, layer: LayerID)>;
type AclChangeCallback = Option<extern "C" fn(ctx: *mut c_void, changes: AclChange)>;
type RecordingStateCallback = Option<extern "C" fn(ctx: *mut c_void, recording: bool)>;

/// A copy of the layerstack to use in the main thread while the
/// paint engine is busy
pub struct ViewCache {
    pub layerstack: Arc<LayerStack>,
    snapshots: SnapshotQueue,
    unrefreshed_area: AoE,
}

/// The callbacks used to (asynchronously) notify the view layer of
/// changes to the canvas.
/// Note that these callbacks are called in the *paint engine thread*.
/// They should only be used to post events to the main thread's event loop.
struct CanvasChangeCallbacks {
    context_object: *mut c_void,
    notify_changes: NotifyChangesCallback,
    notify_resize: NotifyResizeCallback,
    notify_layerlist: NotifyLayerListCallback,
    notify_annotations: NotifyAnnotationsCallback,
    notify_cursor: NotifyCursorCallback,
    notify_playback: NotifyPlaybackCallback,
    notify_catchup: NotifyCatchupCallback,
    notify_metadata: NotifyMetadataCallback,
    notify_timeline: NotifyTimelineCallback,
}

// Unsafe due to the c_void pointer. We promise to be careful with it.
unsafe impl Send for CanvasChangeCallbacks {}

/// Commands sent to the paint engine
enum PaintEngineCommand {
    LocalMessage(CommandMessage),
    RemoteMessage(CommandMessage),
    BrushPreview(LayerID, Vec<CommandMessage>),
    RemovePreview(LayerID),
    ReplaceCanvas(Box<LayerStack>),
    PlaybackPaused(i64, u32), // triggers PlaybackCallback
    SetLocalVisibility(LayerID, bool),
    TruncateHistory,
    Cleanup,
    Catchup(u32),
}

/// The paint engine
pub struct PaintEngine {
    /// A copy of the canvas state for read-only use in the view layer
    pub viewcache: Arc<Mutex<ViewCache>>,

    /// Layer view mode
    view_opts: LayerViewOptions,

    /// A channel for sending commands to the paint engine
    engine_channel: Sender<PaintEngineCommand>,

    /// The paint engine thread's handle
    thread_handle: thread::JoinHandle<()>,

    /// Message filtering state
    aclfilter: AclFilter,

    /// Recorder for received messages
    recorder: Option<Box<dyn rec_writer::RecordingWriter>>,

    /// When was the last message written to the recording.
    /// This is used to insert Interval messages to preserve original
    /// timing information.
    last_recorded_interval: Instant,

    /// Player for playing back a recording
    player: Option<Box<dyn rec_reader::RecordingReader>>,
    player_index: Option<IndexReader>,
    player_path: PathBuf,

    /// View mode changes are done in the main thread
    notify_changes: NotifyChangesCallback,
    notify_cursor: NotifyCursorCallback,
    notify_frame_visibility: NotifyFrameVisibilityCallback,
    context_object: *mut c_void,

    /// Meta message notification context
    meta_context: *mut c_void,
    meta_notify_join: JoinCallback,
    meta_notify_leave: LeaveCallback,
    meta_notify_chat: ChatCallback,
    meta_notify_laser: LaserCallback,
    meta_notify_defaultlayer: DefaultLayerCallback,
    state_notify_aclchange: AclChangeCallback,
    state_notify_recording: RecordingStateCallback,
}

fn run_paintengine(
    viewcache: Arc<Mutex<ViewCache>>,
    channel: Receiver<PaintEngineCommand>,
    callbacks: CanvasChangeCallbacks,
) {
    let mut canvas = CanvasState::new();

    // only the last preview command needs to be executed
    let mut preview: (LayerID, Vec<CommandMessage>) = (0, Vec::new());
    loop {
        // Wait for a command
        let mut cmd = match channel.recv() {
            Ok(m) => m,
            Err(_) => {
                return;
            }
        };

        let mut changes = CanvasStateChange::nothing();

        // Execute all queued commands
        loop {
            use PaintEngineCommand::*;
            match cmd {
                LocalMessage(m) => {
                    changes |= canvas.receive_local_message(&m);
                }
                RemoteMessage(m) => {
                    changes |= canvas.receive_message(&m);
                }
                BrushPreview(layer, commands) => {
                    preview = (layer, commands);
                }
                RemovePreview(layer) => {
                    preview = (0, Vec::new());
                    changes |= canvas.remove_preview(layer);
                }
                ReplaceCanvas(layerstack) => {
                    canvas = CanvasState::new_with_layerstack(*layerstack);
                    changes = CanvasStateChange::everything();
                }
                PlaybackPaused(pos, interval) => {
                    (callbacks.notify_playback)(callbacks.context_object, pos, interval);
                }
                TruncateHistory => {
                    canvas.truncate_history();
                }
                Cleanup => {
                    canvas.cleanup();
                }
                Catchup(progress) => {
                    (callbacks.notify_catchup)(callbacks.context_object, progress);
                }
                SetLocalVisibility(id, visible) => {
                    changes |= canvas.receive_message(&CommandMessage::LayerVisibility(
                        0,
                        LayerVisibilityMessage { id, visible },
                    ));
                }
            };

            cmd = match channel.try_recv() {
                Ok(m) => m,
                Err(mpsc::TryRecvError::Empty) => {
                    break;
                }
                Err(mpsc::TryRecvError::Disconnected) => {
                    return;
                }
            };
        }

        // Execute the last issue preview command (if any)
        // Every preview command overwrites the preview layer,
        // so only the last one needs to be rendered.
        if !preview.1.is_empty() {
            changes |= canvas.apply_preview(preview.0, &preview.1);
        }

        // Notify changes
        if changes.has_any() {
            {
                let mut vc = viewcache.lock().unwrap();
                let new_layerstack = canvas.arc_layerstack();
                vc.snapshots.add(&new_layerstack);
                vc.layerstack = new_layerstack;

                match changes.aoe {
                    AoE::Nothing => {}
                    AoE::Resize(rx, ry, original_size) => {
                        vc.unrefreshed_area = AoE::Everything;
                        (callbacks.notify_resize)(callbacks.context_object, rx, ry, original_size);
                    }
                    _ => {
                        let canvas_size = canvas.layerstack().root().size();
                        if let Some(bounds) = changes.aoe.bounds(canvas_size) {
                            let unrefreshed_area =
                                std::mem::replace(&mut vc.unrefreshed_area, AoE::Nothing);
                            vc.unrefreshed_area = unrefreshed_area.merge(changes.aoe);
                            (callbacks.notify_changes)(callbacks.context_object, bounds);
                        }
                    }
                }
            }

            if changes.layers_changed {
                let layers = flatten_layerinfo(canvas.layerstack().root());
                (callbacks.notify_layerlist)(
                    callbacks.context_object,
                    layers.as_ptr(),
                    layers.len(),
                );
            }

            if changes.annotations_changed {
                let annotations = Box::new(Annotations(canvas.layerstack().get_annotations()));

                // Important: the user is responsible for calling annotations_free
                (callbacks.notify_annotations)(callbacks.context_object, Box::leak(annotations));
            }

            if changes.metadata_changed {
                (callbacks.notify_metadata)(callbacks.context_object);
            }

            if changes.timeline_changed {
                (callbacks.notify_timeline)(callbacks.context_object);
            }

            if changes.user > 0 {
                (callbacks.notify_cursor)(
                    callbacks.context_object,
                    changes.user,
                    changes.layer,
                    changes.cursor.0,
                    changes.cursor.1,
                );
            }
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
    annotations: NotifyAnnotationsCallback,
    cursors: NotifyCursorCallback,
    playback: NotifyPlaybackCallback,
    catchup: NotifyCatchupCallback,
    metadata: NotifyMetadataCallback,
    timeline: NotifyTimelineCallback,
    framevis: NotifyFrameVisibilityCallback,
) -> *mut PaintEngine {
    let viewcache = Arc::new(Mutex::new(ViewCache {
        layerstack: Arc::new(LayerStack::new(0, 0)),
        snapshots: SnapshotQueue::new(Duration::from_secs(10), 6),
        unrefreshed_area: AoE::Nothing,
    }));

    let callbacks = CanvasChangeCallbacks {
        context_object: ctx,
        notify_changes: changes,
        notify_resize: resizes,
        notify_layerlist: layers,
        notify_annotations: annotations,
        notify_cursor: cursors,
        notify_playback: playback,
        notify_catchup: catchup,
        notify_metadata: metadata,
        notify_timeline: timeline,
    };

    let (sender, receiver) = mpsc::channel::<PaintEngineCommand>();

    let dp = Box::new(PaintEngine {
        viewcache: viewcache.clone(),
        view_opts: LayerViewOptions::default().with_background(Tile::new_checkerboard(
            &Color::WHITE,
            &Color {
                r: 0.5,
                g: 0.5,
                b: 0.5,
                a: 1.0,
            },
        )),
        engine_channel: sender,
        thread_handle: thread::spawn(move || {
            run_paintengine(viewcache, receiver, callbacks);
        }),
        aclfilter: AclFilter::new(),
        recorder: None,
        last_recorded_interval: Instant::now(),
        player: None,
        player_index: None,
        player_path: PathBuf::new(),
        notify_changes: changes,
        notify_cursor: cursors,
        notify_frame_visibility: framevis,
        context_object: ctx,
        meta_context: ptr::null_mut(),
        meta_notify_join: None,
        meta_notify_leave: None,
        meta_notify_chat: None,
        meta_notify_laser: None,
        meta_notify_defaultlayer: None,
        state_notify_aclchange: None,
        state_notify_recording: None,
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

/// Register callbacks for Meta messages
#[no_mangle]
pub extern "C" fn paintengine_register_meta_callbacks(
    dp: &mut PaintEngine,
    ctx: *mut c_void,
    join: JoinCallback,
    leave: LeaveCallback,
    chat: ChatCallback,
    laser: LaserCallback,
    defaultlayer: DefaultLayerCallback,
    aclchange: AclChangeCallback,
    recordingstate: RecordingStateCallback,
) {
    dp.meta_context = ctx;
    dp.meta_notify_join = join;
    dp.meta_notify_leave = leave;
    dp.meta_notify_chat = chat;
    dp.meta_notify_laser = laser;
    dp.meta_notify_defaultlayer = defaultlayer;
    dp.state_notify_aclchange = aclchange;
    dp.state_notify_recording = recordingstate;
}

/// Get the current size of the canvas.
#[no_mangle]
pub extern "C" fn paintengine_canvas_size(dp: &PaintEngine) -> Size {
    let vc = dp.viewcache.lock().unwrap();
    vc.layerstack.root().size()
}

/// Receive one or more messages
/// Returns false if the paint engine thread has panicked
#[no_mangle]
pub extern "C" fn paintengine_receive_messages(
    dp: &mut PaintEngine,
    local: bool,
    messages: *const u8,
    messages_len: usize,
) -> bool {
    let mut reader = MessageReader::new(unsafe { slice::from_raw_parts(messages, messages_len) });
    let mut aclchanges = 0;

    loop {
        let msg = match Message::read(&mut reader) {
            Result::Ok(m) => m,
            Result::Err(DeserializationError::NoMoreMessages) => {
                break;
            }
            Result::Err(e) => {
                warn!("Received invalid message: {}", e);
                break;
            }
        };

        let (ok, changes) = dp.aclfilter.filter_message(&msg);

        if !ok {
            warn!("Filtered out {}", msg.as_text());
            // TODO write in recording under the "filtered" type?
            continue;
        }

        if !local {
            if let Some(recorder) = &mut dp.recorder {
                if let Err(err) = recorder.write_message(&msg) {
                    warn!("Error writing to recording: {}", err);
                    dp.recorder = None;
                    if let Some(cb) = dp.state_notify_recording {
                        (cb)(dp.meta_context, false);
                    }
                } else {
                    let interval = dp.last_recorded_interval.elapsed().as_millis();
                    if interval > 500 {
                        if let Err(err) = recorder.write_message(&Message::ClientMeta(
                            ClientMetaMessage::Interval(0, interval.min(0xffff) as u16),
                        )) {
                            warn!("Error writing interval to recording: {}", err);
                        }
                        dp.last_recorded_interval = Instant::now();
                    }
                }
            }
        }

        aclchanges |= changes;

        match msg {
            Message::Command(m) => {
                if let Err(err) = dp.engine_channel.send(if local {
                    PaintEngineCommand::LocalMessage(m)
                } else {
                    PaintEngineCommand::RemoteMessage(m)
                }) {
                    warn!("Couldn't send command to paint engine thread {:?}", err);
                    return false;
                }
            }
            Message::ServerMeta(m) => {
                if !local {
                    dp.handle_servermeta(m);
                }
            }
            Message::ClientMeta(m) => {
                if !local {
                    dp.handle_clientmeta(m);
                }
            }
            Message::Control(_) => (),
        };
    }

    if aclchanges != 0 {
        if let Some(cb) = dp.state_notify_aclchange {
            (cb)(dp.meta_context, aclchanges);
        }
    }

    true
}

/// Enqueue a "catch up" progress marker
///
/// This is used to synchronize a progress bar to the
/// progress of the message queue
#[no_mangle]
pub extern "C" fn paintengine_enqueue_catchup(dp: &mut PaintEngine, progress: u32) -> bool {
    if let Err(err) = dp
        .engine_channel
        .send(PaintEngineCommand::Catchup(progress))
    {
        warn!(
            "Couldn't send catchup command to paint engine thread {:?}",
            err
        );
        return false;
    }

    true
}

/// Clean up the paint engine state after disconnecting from a session
#[no_mangle]
pub extern "C" fn paintengine_cleanup(dp: &mut PaintEngine) -> bool {
    if let Err(err) = dp.engine_channel.send(PaintEngineCommand::Cleanup) {
        warn!(
            "Couldn't send cleanup command to paint engine thread {:?}",
            err
        );
        return false;
    }

    true
}

/// Reset the canvas in preparation of receiving a reset image
#[no_mangle]
pub extern "C" fn paintengine_reset_canvas(dp: &mut PaintEngine) {
    dp.aclfilter = AclFilter::new();

    if let Err(err) = dp
        .engine_channel
        .send(PaintEngineCommand::ReplaceCanvas(Box::new(
            LayerStack::new(0, 0),
        )))
    {
        warn!(
            "Couldn't send reset canvas command to paint engine thread {:?}",
            err
        );
    }

    if let Some(cb) = dp.state_notify_aclchange {
        (cb)(dp.meta_context, 0xff);
    }

    if dp.recorder.is_some() {
        paintengine_stop_recording(dp);
        // TODO automatically start a new recording?
    }
}

/// Reset the ACL filter back to local (non-networked) operating mode
#[no_mangle]
pub extern "C" fn paintengine_reset_acl(dp: &mut PaintEngine, local_user: UserID) {
    dp.aclfilter.reset(local_user);

    if let Some(cb) = dp.state_notify_aclchange {
        (cb)(dp.meta_context, 0xff);
    }
}

/// Get the color of the background tile
///
/// TODO this presently assumes the background tile is always solid.
/// TODO We should support background patterns in the GUI as well.
#[no_mangle]
pub extern "C" fn paintengine_background_color(dp: &PaintEngine) -> Color {
    let vc = dp.viewcache.lock().unwrap();
    vc.layerstack
        .background
        .solid_color()
        .unwrap_or(Color::TRANSPARENT)
}

/// Find the next unused annotation ID for the given user
#[no_mangle]
pub extern "C" fn paintengine_get_available_annotation_id(
    dp: &PaintEngine,
    user: UserID,
) -> AnnotationID {
    let vc = dp.viewcache.lock().unwrap();

    vc.layerstack.find_available_annotation_id(user)
}

/// Find the annotation at the given position
#[no_mangle]
pub extern "C" fn paintengine_get_annotation_at(
    dp: &PaintEngine,
    x: i32,
    y: i32,
    expand: i32,
) -> AnnotationAt {
    let vc = dp.viewcache.lock().unwrap();

    if let Some(a) = vc.layerstack.get_annotation_at(x, y, expand) {
        AnnotationAt {
            id: a.id,
            rect: a.rect,
            protect: a.protect,
        }
    } else {
        AnnotationAt {
            id: 0,
            rect: Rectangle::new(0, 0, 1, 1),
            protect: false,
        }
    }
}

/// Check if the paint engine's content is simple enough to be saved in a flat image
///
/// If any features that requires the OpenRaster file format (such as multiple layers)
/// are used, this will return false.
#[no_mangle]
pub extern "C" fn paintengine_is_simple(dp: &PaintEngine) -> bool {
    let vc = dp.viewcache.lock().unwrap();
    vc.layerstack.root().layer_count() <= 1
        && vc.layerstack.get_annotations().len() == 0
        && vc.layerstack.background.is_blank()
}

/// Get an integer type metadata field
#[no_mangle]
pub extern "C" fn paintengine_get_metadata_int(dp: &PaintEngine, field: MetadataInt) -> i32 {
    let vc = dp.viewcache.lock().unwrap();
    let md = vc.layerstack.metadata();

    match field {
        MetadataInt::Dpix => md.dpix,
        MetadataInt::Dpiy => md.dpiy,
        MetadataInt::Framerate => md.framerate,
        MetadataInt::UseTimeline => md.use_timeline.into(),
    }
}

#[no_mangle]
pub extern "C" fn paintengine_set_view_mode(
    dp: &mut PaintEngine,
    mode: LayerViewMode,
    censor: bool,
) {
    if dp.view_opts.viewmode != mode || dp.view_opts.censor != censor {
        dp.view_opts.viewmode = mode;
        dp.view_opts.censor = censor;
        let aoe_bounds = {
            let mut vc = dp.viewcache.lock().unwrap();
            vc.unrefreshed_area = AoE::Everything;
            vc.unrefreshed_area.bounds(vc.layerstack.root().size())
        };

        if let Some(r) = aoe_bounds {
            (dp.notify_changes)(dp.context_object, r);
        }

        (dp.notify_frame_visibility)(
            dp.context_object,
            &dp.view_opts.active_frame,
            matches!(
                dp.view_opts.viewmode,
                LayerViewMode::Frame | LayerViewMode::Onionskin
            ),
        );
    }
}

#[no_mangle]
pub extern "C" fn paintengine_is_censored(dp: &PaintEngine) -> bool {
    dp.view_opts.censor
}

#[no_mangle]
pub extern "C" fn paintengine_set_onionskin_opts(
    dp: &mut PaintEngine,
    skins_below: usize,
    skins_above: usize,
    tint: bool,
) {
    if skins_below != dp.view_opts.frames_below.len()
        || skins_above != dp.view_opts.frames_above.len()
        || tint != dp.view_opts.onionskin_tint
    {
        dp.view_opts.frames_below = vec![Frame::empty(); skins_below];
        dp.view_opts.frames_above = vec![Frame::empty(); skins_above];
        dp.view_opts.onionskin_tint = tint;

        if dp.view_opts.viewmode != LayerViewMode::Onionskin {
            return;
        }

        let aoe_bounds = {
            let mut vc = dp.viewcache.lock().unwrap();
            vc.unrefreshed_area = AoE::Everything;
            vc.unrefreshed_area.bounds(vc.layerstack.root().size())
        };

        if let Some(r) = aoe_bounds {
            (dp.notify_changes)(dp.context_object, r);
        }
    }
}

#[no_mangle]
pub extern "C" fn paintengine_set_active_layer(dp: &mut PaintEngine, layer_id: LayerID) {
    let aoe_bounds = {
        let mut vc = dp.viewcache.lock().unwrap();

        let changed = match dp.view_opts.viewmode {
            LayerViewMode::Solo => dp.view_opts.active_layer_id != layer_id,
            _ => false,
        };

        dp.view_opts.active_layer_id = layer_id;

        if !changed || vc.layerstack.root().width() == 0 {
            None
        } else {
            vc.unrefreshed_area = AoE::Everything;
            vc.unrefreshed_area.bounds(vc.layerstack.root().size())
        }
    };

    if let Some(r) = aoe_bounds {
        (dp.notify_changes)(dp.context_object, r);
    }
}

#[no_mangle]
pub extern "C" fn paintengine_set_active_frame(dp: &mut PaintEngine, frame_idx: isize) {
    let aoe_bounds = {
        let mut vc = dp.viewcache.lock().unwrap();

        let frame_idx = frame_idx.clamp(1, vc.layerstack.frame_count().max(1) as isize) - 1;

        let frame = vc.layerstack.frame_at(frame_idx);

        let frames_below = dp.view_opts.frames_below.len() as isize;
        for i in 0..frames_below {
            dp.view_opts.frames_below[i as usize] = vc.layerstack.frame_at(frame_idx - i - 1);
        }
        let frames_above = dp.view_opts.frames_above.len() as isize;
        for i in 0..frames_above {
            dp.view_opts.frames_above[i as usize] = vc.layerstack.frame_at(frame_idx + i + 1);
        }

        let changed = match dp.view_opts.viewmode {
            LayerViewMode::Frame | LayerViewMode::Onionskin => dp.view_opts.active_frame != frame,
            _ => false,
        };

        dp.view_opts.active_frame = frame;

        if !changed || vc.layerstack.root().width() == 0 {
            None
        } else {
            vc.unrefreshed_area = AoE::Everything;
            vc.unrefreshed_area.bounds(vc.layerstack.root().size())
        }
    };

    if let Some(r) = aoe_bounds {
        (dp.notify_changes)(dp.context_object, r);
        (dp.notify_frame_visibility)(
            dp.context_object,
            &dp.view_opts.active_frame,
            matches!(
                dp.view_opts.viewmode,
                LayerViewMode::Frame | LayerViewMode::Onionskin
            ),
        );
    }
}

/// Check the given coordinates and return the ID of the user
/// who last touched the tile under it.
/// This will also set that user ID as the canvas highlight ID.
#[no_mangle]
pub extern "C" fn paintengine_inspect_canvas(dp: &mut PaintEngine, x: i32, y: i32) -> UserID {
    let (user, aoe_bounds) = {
        let mut vc = dp.viewcache.lock().unwrap();
        let user = vc.layerstack.root().last_edited_by(x, y);

        if dp.view_opts.highlight != user {
            dp.view_opts.highlight = user;
            vc.unrefreshed_area = AoE::Everything;
            (
                user,
                vc.unrefreshed_area.bounds(vc.layerstack.root().size()),
            )
        } else {
            (user, None)
        }
    };

    if let Some(r) = aoe_bounds {
        (dp.notify_changes)(dp.context_object, r);
    }

    user
}

/// Set the canvas inspect target user ID.
/// When set, all tiles last  touched by this use will be highlighted.
/// Setting this to zero switches off inspection mode.
#[no_mangle]
pub extern "C" fn paintengine_set_highlight_user(dp: &mut PaintEngine, user: UserID) {
    if user != dp.view_opts.highlight {
        dp.view_opts.highlight = user;
        let aoe_bounds = {
            let mut vc = dp.viewcache.lock().unwrap();
            if vc.layerstack.root().width() == 0 {
                None
            } else {
                vc.unrefreshed_area = AoE::Everything;
                vc.unrefreshed_area.bounds(vc.layerstack.root().size())
            }
        };

        if let Some(r) = aoe_bounds {
            (dp.notify_changes)(dp.context_object, r);
        }
    }
}

/// Set a layer's local visibility flag
#[no_mangle]
pub extern "C" fn paintengine_set_layer_visibility(
    dp: &mut PaintEngine,
    layer_id: LayerID,
    visible: bool,
) {
    if let Err(err) = dp
        .engine_channel
        .send(PaintEngineCommand::SetLocalVisibility(layer_id, visible))
    {
        warn!(
            "Couldn't send visibility command to paint engine: {:?}",
            err
        );
    }
}

/// Draw a preview brush stroke onto the given layer
///
/// This consumes the content of the brush engine.
#[no_mangle]
pub extern "C" fn paintengine_preview_brush(
    dp: &mut PaintEngine,
    layer_id: LayerID,
    brushengine: &mut BrushEngine,
) {
    if let Err(err) = dp.engine_channel.send(PaintEngineCommand::BrushPreview(
        layer_id,
        brushengine.take_dabs(0),
    )) {
        warn!("Couldn't send preview strokes to paint engine: {:?}", err);
    }
}

/// Make a temporary eraser layer to preview a cut operation
#[no_mangle]
pub extern "C" fn paintengine_preview_cut(
    dp: &mut PaintEngine,
    layer_id: LayerID,
    rect: Rectangle,
    mask: *const u8,
) {
    let cmd = if mask.is_null() {
        vec![CommandMessage::FillRect(
            0,
            FillRectMessage {
                layer: 0,
                mode: Blendmode::Erase.into(),
                x: rect.x as u32,
                y: rect.y as u32,
                w: rect.w as u32,
                h: rect.h as u32,
                color: 0xff_ffffff,
            },
        )]
    } else {
        // TODO an ImageRef struct so we could do this without copying
        let mut maskimg = Image8::new(rect.w as usize, rect.h as usize);
        maskimg.pixels[..].copy_from_slice(unsafe {
            slice::from_raw_parts_mut(mask as *mut Pixel8, (rect.w * rect.h) as usize)
        });

        make_putimage(
            0,
            0,
            rect.x as u32,
            rect.y as u32,
            &maskimg,
            Blendmode::Erase,
        )
    };

    if let Err(err) = dp
        .engine_channel
        .send(PaintEngineCommand::BrushPreview(layer_id, cmd))
    {
        warn!("Couldn't send preview strokes to paint engine: {:?}", err);
    }
}

/// Remove preview brush strokes from the given layer
/// If layer ID is 0, previews from all layers will be removed
#[no_mangle]
pub extern "C" fn paintengine_remove_preview(dp: &mut PaintEngine, layer_id: LayerID) {
    if let Err(err) = dp
        .engine_channel
        .send(PaintEngineCommand::RemovePreview(layer_id))
    {
        warn!("Couldn't send remove preview to paint engine: {:?}", err);
    }
}

/// Perform a flood fill operation
///
/// Returns false if the operation fails (e.g. due to size limit.)
#[no_mangle]
pub extern "C" fn paintengine_floodfill(
    dp: &mut PaintEngine,
    writer: &mut MessageWriter,
    user_id: UserID,
    layer_id: LayerID,
    x: i32,
    y: i32,
    color: Color,
    tolerance: f32,
    sample_merged: bool,
    size_limit: u32,
    expansion: i32,
    fill_under: bool,
) -> bool {
    let mut result = {
        let vc = dp.viewcache.lock().unwrap();
        floodfill::floodfill(
            &vc.layerstack,
            x,
            y,
            color,
            tolerance,
            layer_id,
            sample_merged,
            size_limit,
        )
    };

    if result.image.is_null() || result.oversize {
        return false;
    }

    if expansion > 0 {
        result = floodfill::expand_floodfill(result, expansion);
    }

    // Flood fill is implemented using PutImage rather than a native command.
    // This has the following advantages:
    // - backward and forward compatibility: changes in the algorithm can be made freely
    // - tolerates out-of-sync canvases (shouldn't normally happen, but...)
    // - bugs don't crash/freeze other clients
    //
    // The disadvantage is increased bandwith consumption. However, this is not as bad
    // as one might think: the effective bit-depth of the bitmap is 1bpp and most fills
    // consist of large solid areas, meaning they should compress ridiculously well.

    messages_ffi::write_undopoint(writer, user_id);

    // Note: If the target area is transparent, use the BEHIND compositing mode.
    // This results in nice smooth blending with soft outlines, when the
    // outline's color is different from the fill.

    make_putimage(
        user_id,
        layer_id,
        result.x as u32,
        result.y as u32,
        &result.image,
        if color.a == 0.0 {
            Blendmode::Erase
        } else if fill_under || result.layer_seed_color.a == 0.0 {
            Blendmode::Behind
        } else {
            Blendmode::Normal
        },
    )
    .iter()
    .for_each(|cmd| cmd.write(writer));

    true
}

/// Generate the layer reordering command for moving layer A to
/// a position above layer B (or into it, if it is a group)
///
/// Returns false if a move command couldn't be generated
#[no_mangle]
pub extern "C" fn paintengine_make_movelayer(
    dp: &mut PaintEngine,
    writer: &mut MessageWriter,
    user_id: UserID,
    source_layer: LayerID,
    target_layer: LayerID,
    into_group: bool,
    below: bool,
) -> bool {
    // TODO find closest common root to generate minimal
    // re-ordering and permit reordering of limited access groups.
    let new_ordering = {
        let vc = dp.viewcache.lock().unwrap();
        editstack::move_ordering(
            vc.layerstack.root(),
            source_layer,
            target_layer,
            into_group,
            below,
        )
    };

    if let Some(layers) = new_ordering {
        CommandMessage::LayerOrder(user_id, LayerOrderMessage { root: 0, layers }).write(writer);
        return true;
    }

    false
}

/// Generate the commands for deleting all the empty
/// annotations presently on the canvas
#[no_mangle]
pub extern "C" fn paintengine_make_delete_empty_annotations(
    dp: &mut PaintEngine,
    writer: &mut MessageWriter,
    user_id: UserID,
) {
    let vc = dp.viewcache.lock().unwrap();
    for a in vc.layerstack.get_annotations().iter() {
        if a.text.is_empty() {
            messages_ffi::write_deleteannotation(writer, user_id, a.id);
        }
    }
}

/// Pick a color from the canvas
///
/// If the given layer ID is 0, color is taken from merged layers
#[no_mangle]
pub extern "C" fn paintengine_sample_color(
    dp: &PaintEngine,
    x: i32,
    y: i32,
    layer_id: LayerID,
    dia: i32,
) -> Color {
    let vc = dp.viewcache.lock().unwrap();

    if layer_id > 0 {
        if let Some(layer) = vc.layerstack.root().get_bitmaplayer(layer_id) {
            layer.sample_color(x, y, dia)
        } else {
            Color::TRANSPARENT
        }
    } else {
        vc.layerstack.sample_color(x, y, dia)
    }
}

/// Find the topmost layer at the given coordinates
///
#[no_mangle]
pub extern "C" fn paintengine_pick_layer(dp: &PaintEngine, x: i32, y: i32) -> LayerID {
    let vc = dp.viewcache.lock().unwrap();

    vc.layerstack.root().pick_layer(x, y)
}

/// Find the bounding rectangle of the layer's content
///
/// Returns a zero rectangle if entire layer is blank
#[no_mangle]
pub extern "C" fn paintengine_get_layer_bounds(dp: &PaintEngine, layer_id: LayerID) -> Rectangle {
    let vc = dp.viewcache.lock().unwrap();

    if let Some(layer) = vc.layerstack.root().get_bitmaplayer(layer_id) {
        if let Some(r) = layer.find_bounds() {
            return r;
        }
    }

    Rectangle {
        x: 0,
        y: 0,
        w: 0,
        h: 0,
    }
}

/// Copy layer pixel data to the given buffer
///
/// The rectangle must be contained within the layer bounds.
/// The size if the buffer must be rect.w * rect.h * 4 bytes.
/// If the copy operation fails, false will be returned.
///
/// If the layer ID is 0, the layers (background included)
/// will be flattened
/// Layer ID if -1 will flatten the layers but not include the
/// canvas background.
#[no_mangle]
pub extern "C" fn paintengine_get_layer_content(
    dp: &PaintEngine,
    layer_id: i32,
    rect: Rectangle,
    pixels: *mut u8,
) -> bool {
    let vc = dp.viewcache.lock().unwrap();
    if !rect.in_bounds(vc.layerstack.root().size()) {
        return false;
    }

    let pixel_slice =
        unsafe { slice::from_raw_parts_mut(pixels as *mut Pixel8, (rect.w * rect.h) as usize) };

    if layer_id < 0 {
        let mut vopts = LayerViewOptions::default();
        vopts.no_canvas_background = true;
        vc.layerstack.to_pixels8(rect, &vopts, pixel_slice).is_ok()
    } else if layer_id == 0 {
        vc.layerstack
            .to_pixels8(rect, &LayerViewOptions::default(), pixel_slice)
            .is_ok()
    } else if let Ok(layer_id) = LayerID::try_from(layer_id) {
        vc.layerstack
            .root()
            .get_bitmaplayer(layer_id)
            .and_then(|l| l.to_pixels8(rect, pixel_slice).ok())
            .is_some()
    } else {
        false
    }
}

/// Get the number of frames in the layerstack
///
/// When the layerstack is treated as an animation,
/// each non-fixed layer is treated as a frame, therefore
/// the number of frames can be less than the number of layers
/// in the stack.
#[no_mangle]
pub extern "C" fn paintengine_get_frame_count(dp: &PaintEngine) -> usize {
    let vc = dp.viewcache.lock().unwrap();

    vc.layerstack.frame_count()
}

type GetTimelineCallback = extern "C" fn(ctx: *mut c_void, frames: *const Frame, count: usize);

/// Get the animation timeline
#[no_mangle]
pub extern "C" fn paintengine_get_timeline(
    dp: &PaintEngine,
    ctx: *mut c_void,
    cb: GetTimelineCallback,
) {
    let vc = dp.viewcache.lock().unwrap();
    let timeline = vc.layerstack.timeline();

    (cb)(ctx, timeline.frames.as_ptr(), timeline.frames.len());
}

/// Copy frame pixel data to the given buffer
///
/// This works like paintengine_get_layer_content, with two
/// differences:
///
///  * frame index is used instead of layer ID
///  * background is composited
///  * fixed layers are composited
#[no_mangle]
pub extern "C" fn paintengine_get_frame_content(
    dp: &PaintEngine,
    frame: isize,
    rect: Rectangle,
    pixels: *mut u8,
) -> bool {
    let vc = dp.viewcache.lock().unwrap();
    if !rect.in_bounds(vc.layerstack.root().size()) {
        return false;
    }

    let pixel_slice =
        unsafe { slice::from_raw_parts_mut(pixels as *mut Pixel8, (rect.w * rect.h) as usize) };

    let opts = LayerViewOptions::frame(vc.layerstack.frame_at(frame));

    vc.layerstack.to_pixels8(rect, &opts, pixel_slice).is_ok()
}

/// Get a snapshot of the canvas state to use as a reset image
#[no_mangle]
pub extern "C" fn paintengine_get_reset_snapshot(dp: &mut PaintEngine, writer: &mut MessageWriter) {
    let snapshot = {
        let vc = dp.viewcache.lock().unwrap();
        make_canvas_snapshot(1, &vc.layerstack, 0, Some(&dp.aclfilter))
    };

    for msg in snapshot {
        msg.write(writer);
    }
}

/// Get a snapshot of a past canvas state to use as a reset image
#[no_mangle]
pub extern "C" fn paintengine_get_historical_reset_snapshot(
    dp: &PaintEngine,
    snapshots: &SnapshotQueue,
    index: usize,
    writer: &mut MessageWriter,
) -> bool {
    if let Some(snapshot) = snapshots.get(index) {
        let msgs = make_canvas_snapshot(1, snapshot, 0, Some(&dp.aclfilter));

        for msg in msgs {
            msg.write(writer);
        }

        true
    } else {
        false
    }
}

#[no_mangle]
pub extern "C" fn paintengine_get_acl_users(dp: &PaintEngine) -> &UserACLs {
    dp.aclfilter.users()
}

#[no_mangle]
pub extern "C" fn paintengine_get_acl_layers(
    dp: &PaintEngine,
    context: *mut c_void,
    visitor: extern "C" fn(ctx: *mut c_void, id: LayerID, layer: &LayerACL),
) {
    for (&id, acl) in dp.aclfilter.layers().iter() {
        (visitor)(context, id, acl)
    }
}

#[no_mangle]
pub extern "C" fn paintengine_get_acl_features(dp: &PaintEngine) -> &FeatureTiers {
    dp.aclfilter.feature_tiers()
}

/// Get a (shallow) copy of the snapshot buffer
///
/// This is used when opening an UI for picking a snapshot to restore/export/etc
/// so that ongoing canvas activity won't change the buffer while the view is open.
/// The buffer must be released with paintengine_release_snapshots.
#[no_mangle]
pub extern "C" fn paintengine_get_snapshots(dp: &PaintEngine) -> *mut SnapshotQueue {
    let s = {
        let vc = dp.viewcache.lock().unwrap();
        Box::new(vc.snapshots.clone())
    };
    Box::into_raw(s)
}

#[no_mangle]
pub extern "C" fn paintengine_release_snapshots(snapshots: *mut SnapshotQueue) {
    unsafe { Box::from_raw(snapshots) };
}

#[no_mangle]
pub extern "C" fn paintengine_load_blank(
    dp: &mut PaintEngine,
    width: u32,
    height: u32,
    background: Color,
) -> bool {
    let mut ls = Box::new(LayerStack::new(width, height));

    ls.background = Tile::new(&background, 0);
    let l = ls
        .root_mut()
        .add_bitmap_layer(0x0100, Color::TRANSPARENT, LayerInsertion::Top)
        .unwrap();
    l.metadata_mut().title = "Layer 1".into();

    if let Err(err) = dp
        .engine_channel
        .send(PaintEngineCommand::ReplaceCanvas(ls))
    {
        warn!(
            "Couldn't send replace command to paint engine thread {:?}",
            err
        );
        return false;
    }

    true
}

#[repr(C)]
pub enum CanvasIoError {
    NoError,
    FileOpenError,
    FileIoError,
    UnsupportedFormat,
    PartiallySupportedFormat, // file was loaded but incompletely
    UnknownRecordingVersion,  // recording was opened, but degree of compatibility is unknown
    CodecError,
    PaintEngineCrashed,
}

impl From<dpimpex::ImpexError> for CanvasIoError {
    fn from(err: dpimpex::ImpexError) -> Self {
        match err {
            dpimpex::ImpexError::IoError(e) => match e.kind() {
                std::io::ErrorKind::NotFound | std::io::ErrorKind::PermissionDenied => {
                    Self::FileOpenError
                }
                _ => Self::FileIoError,
            },
            dpimpex::ImpexError::UnsupportedFormat => Self::UnsupportedFormat,
            dpimpex::ImpexError::CodecError(_)
            | dpimpex::ImpexError::XmlError(_)
            | dpimpex::ImpexError::NoContent => Self::CodecError,
        }
    }
}

/// Load canvas content from a file
#[no_mangle]
pub extern "C" fn paintengine_load_file(
    dp: &mut PaintEngine,
    path: *const u16,
    path_len: usize,
) -> CanvasIoError {
    let path = String::from_utf16_lossy(unsafe { slice::from_raw_parts(path, path_len) });

    let ls = match dpimpex::load_image(path) {
        Ok(ls) => Box::new(ls),
        Err(err) => {
            warn!("Couldn't load: {:?}", err);
            return err.into();
        }
    };

    if let Err(err) = dp
        .engine_channel
        .send(PaintEngineCommand::ReplaceCanvas(ls))
    {
        warn!(
            "Couldn't send replace command to paint engine thread {:?}",
            err
        );
        return CanvasIoError::PaintEngineCrashed;
    }

    CanvasIoError::NoError
}

/// Open a recording for playback
///
/// This clears the existing canvas, just as loading any other file would.
#[no_mangle]
pub extern "C" fn paintengine_load_recording(
    dp: &mut PaintEngine,
    path: *const u16,
    path_len: usize,
) -> CanvasIoError {
    let path = String::from_utf16_lossy(unsafe { slice::from_raw_parts(path, path_len) });

    let player = match rec_reader::open_recording(Path::new(&path)) {
        Ok(p) => p,
        Err(e) => {
            warn!("Couldn't open recording: {}", e);
            return match e.kind() {
                std::io::ErrorKind::NotFound | std::io::ErrorKind::PermissionDenied => {
                    CanvasIoError::FileOpenError
                }
                _ => CanvasIoError::FileIoError,
            };
        }
    };

    let compatibility = match player.check_compatibility() {
        rec_reader::Compatibility::Compatible => CanvasIoError::NoError,
        rec_reader::Compatibility::MinorDifferences => CanvasIoError::PartiallySupportedFormat,
        rec_reader::Compatibility::Unknown => CanvasIoError::UnknownRecordingVersion,
        rec_reader::Compatibility::Incompatible => {
            warn!("Recording not compatible");
            return CanvasIoError::UnsupportedFormat;
        }
    };

    if let Err(err) = dp
        .engine_channel
        .send(PaintEngineCommand::ReplaceCanvas(Box::new(
            LayerStack::new(0, 0),
        )))
    {
        warn!(
            "Couldn't send replace command to paint engine thread {:?}",
            err
        );
        return CanvasIoError::PaintEngineCrashed;
    }

    dp.player = Some(player);
    dp.player_index = None;
    dp.player_path = path.into();

    compatibility
}

/// Stop recording playback
#[no_mangle]
pub extern "C" fn paintengine_close_recording(dp: &mut PaintEngine) {
    dp.player = None;
    dp.player_index = None;

    // Stopping a recording is similar to logging out of an
    // online session, so do the same cleanup to ensure there
    // are no dangling temporary layers etc.
    paintengine_cleanup(dp);
}

/// Try opening the index file for the currently open recording.
#[no_mangle]
pub extern "C" fn paintengine_load_recording_index(dp: &mut PaintEngine) -> bool {
    if dp.player.is_none() {
        return false;
    }

    let idx_path = dp.player_path.with_extension("dpidx");
    dp.player_index = match IndexReader::open(&idx_path) {
        Ok(r) => Some(r),
        Err(e) => {
            warn!("couldn't load index: {}", e);
            return false;
        }
    };

    true
}

/// Get the number of indexed messages in the recording
#[no_mangle]
pub extern "C" fn paintengine_recording_index_messages(dp: &PaintEngine) -> u32 {
    if let Some(i) = &dp.player_index {
        i.message_count()
    } else {
        0
    }
}

/// Get the number of thumbnails in the recording index
#[no_mangle]
pub extern "C" fn paintengine_recording_index_thumbnails(dp: &PaintEngine) -> u32 {
    if let Some(i) = &dp.player_index {
        i.thumbnail_count()
    } else {
        0
    }
}

/// Get the number of thumbnails in the recording index
#[no_mangle]
pub extern "C" fn paintengine_get_recording_index_thumbnail(
    dp: &mut PaintEngine,
    index: u32,
    length: &mut usize,
) -> *const u8 {
    if let Some(i) = &mut dp.player_index {
        let t = match i.read_thumbnail(index as usize) {
            Ok(t) => t,
            Err(e) => {
                warn!("Couldn't load thumbnail: {}", e);
                *length = 0;
                return ptr::null();
            }
        };

        *length = t.len();
        t.as_ptr()
    } else {
        *length = 0;
        ptr::null()
    }
}

#[no_mangle]
pub extern "C" fn paintengine_build_index(
    dp: &PaintEngine,
    ctx: *mut c_void,
    progress_notification_func: IndexBuildProgressNoticationFn,
) -> bool {
    info!("Building index for {}", dp.player_path.display());

    let started = Instant::now();

    if let Err(err) = build_index(&dp.player_path, ctx, progress_notification_func) {
        warn!("Index building error: {}", err);
        return false;
    }

    info!("Index built in {} seconds", started.elapsed().as_secs());

    true
}

/// Save the currently visible layerstack.
///
/// It is safe to call this function in a separate thread.
#[no_mangle]
pub extern "C" fn paintengine_save_file(
    dp: &PaintEngine,
    path: *const u16,
    path_len: usize,
) -> CanvasIoError {
    let path = String::from_utf16_lossy(unsafe { slice::from_raw_parts(path, path_len) });

    // Grab a copy of the layerstack so we don't block the paint engine thread
    // or the main thread if this is being run as a background process.
    let layerstack = {
        let vc = dp.viewcache.lock().unwrap();
        vc.layerstack.clone()
    };

    if let Err(e) = dpimpex::save_image(&path, &layerstack) {
        warn!("An error occurred while writing \"{}\": {}", path, e);
        return e.into();
    }

    CanvasIoError::NoError
}

#[repr(C)]
pub enum AnimationExportMode {
    Gif,
    Frames,
}

/// Callback for animation saving progress
///
/// This can be used to report back the progress when the save function
/// is run in a separate thread.
///
/// The function should return `false` if the user has requested the cancellation
/// of the export process.
type AnimationExportProgressCallback =
    Option<extern "C" fn(ctx: *mut c_void, progress: f32) -> bool>;

/// Save the layerstack as an animation.
///
/// It is safe to call this function in a separate thread.
#[no_mangle]
pub extern "C" fn paintengine_save_animation(
    dp: &PaintEngine,
    path: *const u16,
    path_len: usize,
    mode: AnimationExportMode,
    callback_ctx: *mut c_void,
    callback: AnimationExportProgressCallback,
) -> CanvasIoError {
    let path = String::from_utf16_lossy(unsafe { slice::from_raw_parts(path, path_len) });

    let layerstack = {
        let vc = dp.viewcache.lock().unwrap();
        vc.layerstack.clone()
    };

    let expected_frames = layerstack.frame_count();

    if expected_frames == 0 {
        warn!("No frames to export!");
        return CanvasIoError::NoError;
    }

    let mut saver: Box<dyn dpimpex::animation::AnimationWriter> = match mode {
        AnimationExportMode::Gif => match dpimpex::animation::GifWriter::new(path.as_ref()) {
            Ok(w) => Box::new(dpimpex::animation::AnimationSaver::new(layerstack, w)),
            Err(e) => {
                warn!("An error occurred while opening \"{}\": {}", path, e);
                return e.into();
            }
        },
        AnimationExportMode::Frames => Box::new(dpimpex::animation::AnimationSaver::new(
            layerstack,
            dpimpex::animation::FrameImagesWriter::new(path.as_ref()),
        )),
    };

    let mut written_frames = 0.0;
    loop {
        written_frames += 1.0;
        match saver.save_next_frame() {
            Ok(true) => {
                if let Some(cb) = callback {
                    if !(cb)(callback_ctx, written_frames / expected_frames as f32) {
                        break;
                    }
                }
            }
            Ok(false) => {
                if let Some(cb) = callback {
                    (cb)(callback_ctx, written_frames / expected_frames as f32);
                }
                break;
            }
            Err(e) => {
                warn!("An error occurred while writing \"{}\": {}", path, e);
                return e.into();
            }
        }
    }

    CanvasIoError::NoError
}

#[no_mangle]
pub extern "C" fn paintengine_start_recording(
    dp: &mut PaintEngine,
    path: *const u16,
    path_len: usize,
) -> CanvasIoError {
    let path = String::from_utf16_lossy(unsafe { slice::from_raw_parts(path, path_len) });

    let textmode = path.ends_with(".dptxt");

    // Open file
    let file = match File::create(path) {
        Ok(f) => f,
        Err(e) => {
            warn!("Couldn't open recording file: {}", e);
            return match e.kind() {
                std::io::ErrorKind::NotFound | std::io::ErrorKind::PermissionDenied => {
                    CanvasIoError::FileOpenError
                }
                _ => CanvasIoError::FileIoError,
            };
        }
    };

    // Open the right kind of recorder
    let mut writer: Box<dyn rec_writer::RecordingWriter> = if textmode {
        Box::new(rec_writer::TextWriter::open(file))
    } else {
        Box::new(rec_writer::BinaryWriter::open(file))
    };

    // Write header
    let mut header = HashMap::new();
    header.insert("version".to_string(), PROTOCOL_VERSION.to_string());
    header.insert("writerversion".to_string(), DRAWPILE_VERSION.to_string());

    if let Err(e) = writer.write_header(&header) {
        warn!("Couldn't writer recording header: {}", e);
        return CanvasIoError::FileIoError;
    }

    // TODO sync the paint engine thread to make sure we get everything

    // Write current canvas content
    let snapshot = {
        let vc = dp.viewcache.lock().unwrap();
        make_canvas_snapshot(1, &vc.layerstack, 0, Some(&dp.aclfilter))
    };

    for msg in snapshot {
        if let Err(e) = writer.write_message(&msg) {
            warn!("Couldn't write snapshot message: {}", e);
            return CanvasIoError::FileIoError;
        }
    }

    // Ready to record
    dp.recorder = Some(writer);
    dp.last_recorded_interval = Instant::now();

    if let Some(cb) = dp.state_notify_recording {
        (cb)(dp.meta_context, true);
    }

    CanvasIoError::NoError
}

#[no_mangle]
pub extern "C" fn paintengine_stop_recording(dp: &mut PaintEngine) {
    dp.recorder = None;
    if let Some(cb) = dp.state_notify_recording {
        (cb)(dp.meta_context, false);
    }
}

/// Play back one or more messages from the recording under playback.
///
/// If "sequences" is true, whole undo sequences are stepped instead of
/// single messages. The playback callback will be called at the end.
#[no_mangle]
pub extern "C" fn paintengine_playback_step(dp: &mut PaintEngine, mut steps: i32, sequences: bool) {
    if steps < 0 {
        let pos = dp.player.as_deref().unwrap().current_index();
        if pos > 0 {
            paintengine_playback_jump(dp, (pos - 1) as u32, false);
        }

        return;
    }

    let mut interval: u32 = 0;
    while steps > 0 {
        let msg = match dp.player.as_deref_mut().unwrap().read_next() {
            rec_reader::ReadMessage::Ok(m) => m,
            rec_reader::ReadMessage::Invalid(m) => {
                warn!("Read an invalid message: {}", m);
                continue;
            }
            rec_reader::ReadMessage::IoError(e) => {
                warn!("An IO error occurred while playing back recording: {}", e);
                return;
            }
            rec_reader::ReadMessage::Eof => {
                if let Err(err) = dp
                    .engine_channel
                    .send(PaintEngineCommand::PlaybackPaused(-1, 0))
                {
                    warn!("Couldn't send command to paint engine thread {:?}", err);
                    return;
                }
                return;
            }
        };

        match msg {
            Message::Command(CommandMessage::UndoPoint(_)) => {
                steps -= 1;
            }
            Message::ClientMeta(ClientMetaMessage::Interval(_, i)) => {
                interval += i as u32;
                steps -= 1;
            }
            Message::Command(_) => {
                if !sequences {
                    steps -= 1;
                }
            }
            _ => (),
        }

        // TODO ACL filtering?

        match msg {
            Message::Command(m) => {
                if let Err(err) = dp.engine_channel.send(PaintEngineCommand::RemoteMessage(m)) {
                    warn!("Couldn't send command to paint engine thread {:?}", err);
                    return;
                }
            }
            Message::ServerMeta(m) => {
                dp.handle_servermeta(m);
            }
            Message::ClientMeta(m) => {
                dp.handle_clientmeta(m);
            }
            Message::Control(_) => (),
        };
    }

    // Notify end-of-step so the playback controller GUI can re-enable the
    // play button (or autoplay the next sequence)
    let player = dp.player.as_deref().unwrap();

    let pos = if dp.player_index.is_some() {
        player.current_index() as i64
    } else {
        // without an index, we don't know how many messages there
        // are in the recording, so we just report the position
        // as a percentage.
        (player.current_progress() * 100.0) as i64
    };

    if let Err(err) = dp
        .engine_channel
        .send(PaintEngineCommand::PlaybackPaused(pos, interval))
    {
        warn!("Couldn't send command to paint engine thread {:?}", err);
    }
}

/// Jump to a position in the recording.
///
/// The recording must have been indexed.
#[no_mangle]
pub extern "C" fn paintengine_playback_jump(dp: &mut PaintEngine, pos: u32, exact: bool) {
    if let Some(index) = &mut dp.player_index {
        let (msg_idx, msg_offset, layerstack) = match index.load_snapshot(pos) {
            Ok(s) => s,
            Err(e) => {
                warn!("Couldn't jump to index {} in recording: {}", pos, e);
                return;
            }
        };

        assert!(pos >= msg_idx);

        let current_index = dp.player.as_deref().unwrap().current_index() as u32;
        if current_index >= msg_idx && current_index < pos {
            // No need to reset if we're already within the snapshot
            paintengine_playback_step(dp, (pos - current_index) as i32, false);
            return;
        }

        if let Err(err) = dp
            .engine_channel
            .send(PaintEngineCommand::ReplaceCanvas(Box::new(layerstack)))
        {
            warn!(
                "Couldn't send reset canvas command to paint engine thread {:?}",
                err
            );
            return;
        }

        dp.player
            .as_deref_mut()
            .unwrap()
            .seek_to(msg_idx, msg_offset);

        if exact && pos > msg_idx {
            paintengine_playback_step(dp, (pos - msg_idx) as i32, false);
        } else if let Err(err) = dp
            .engine_channel
            .send(PaintEngineCommand::PlaybackPaused(msg_idx as i64, 0))
        {
            warn!("Couldn't send command to paint engine thread {:?}", err);
        }
    }
}

#[no_mangle]
pub extern "C" fn paintengine_is_recording(dp: &PaintEngine) -> bool {
    dp.recorder.is_some()
}

#[no_mangle]
pub extern "C" fn paintengine_is_playing(dp: &PaintEngine) -> bool {
    dp.player.is_some()
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

    if vc.layerstack.root().width() < 1 {
        return;
    }

    let intersection = if rect.w < 0 {
        // We interpret an invalid rectangle to mean "refresh everything"
        std::mem::replace(&mut vc.unrefreshed_area, AoE::Nothing)
    } else {
        let ls = vc.layerstack.root().size();
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

    let mut pixels = [ZERO_PIXEL8; TILE_LENGTH];
    FlattenedTileIterator::new(&vc.layerstack, &dp.view_opts, intersection).for_each(
        |(i, j, t)| {
            pixels15_to_8(&mut pixels, &t.pixels);
            paint_func(
                ctx,
                i * TILE_SIZEI,
                j * TILE_SIZEI,
                pixels.as_ptr() as *const u8,
            )
        },
    );
}

impl PaintEngine {
    fn handle_servermeta(&mut self, msg: ServerMetaMessage) {
        use ServerMetaMessage::*;
        match msg {
            Join(u, m) => {
                if let Some(cb) = self.meta_notify_join {
                    (cb)(
                        self.meta_context,
                        u,
                        m.flags,
                        m.name.as_ptr(),
                        m.name.len(),
                        m.avatar.as_ptr(),
                        m.avatar.len(),
                    );
                }
            }
            Leave(u) => {
                if let Some(cb) = self.meta_notify_leave {
                    (cb)(self.meta_context, u);
                }
            }
            Chat(u, m) => {
                if let Some(cb) = self.meta_notify_chat {
                    (cb)(
                        self.meta_context,
                        u,
                        0,
                        m.tflags,
                        m.oflags,
                        m.message.as_ptr(),
                        m.message.len(),
                    );
                }
            }
            PrivateChat(u, m) => {
                if let Some(cb) = self.meta_notify_chat {
                    (cb)(
                        self.meta_context,
                        u,
                        m.target,
                        0,
                        m.oflags,
                        m.message.as_ptr(),
                        m.message.len(),
                    );
                }
            }
            SoftReset(_) => {
                // The (soft) reset point is the beginning of the new history
                if let Err(err) = self
                    .engine_channel
                    .send(PaintEngineCommand::TruncateHistory)
                {
                    warn!("Couldn't send command to paint engine thread {:?}", err);
                }

                // TODO support Thick Server. Send a soft reset trigger command
                // if this user is the resetter.
            }

            // Handled by the ACL filter:
            SessionOwner(_, _) | TrustedUsers(_, _) => (),
        }
    }

    fn handle_clientmeta(&mut self, msg: ClientMetaMessage) {
        use ClientMetaMessage::*;
        match msg {
            LaserTrail(u, m) => {
                if let Some(cb) = self.meta_notify_laser {
                    (cb)(self.meta_context, u, m.persistence, m.color);
                }
            }
            MovePointer(u, m) => {
                (self.notify_cursor)(self.context_object, u, 0, m.x, m.y);
            }
            DefaultLayer(_, m) => {
                if let Some(cb) = self.meta_notify_defaultlayer {
                    (cb)(self.meta_context, m);
                }
            }
            // Handled by the ACL filter:
            UserACL(_, _) | LayerACL(_, _) | FeatureAccessLevels(_, _) => (),

            // Recording stuff:
            Interval(_, _) | Filtered(_, _) => (),
            Marker(_, _) => (), // removed feature
        }
    }
}
