// This file is part of Drawpile.
// Copyright (C) 2020-2021 Calle Laakkonen
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

use super::brushes;
use super::compression;
use super::history::History;
use super::retcon::{LocalFork, RetconAction};
use crate::paint::annotation::{AnnotationID, VAlign};
use crate::paint::{
    editlayer, AoE, BitmapLayer, Blendmode, ClassicBrushCache, Color, Frame, GroupLayer, Layer,
    LayerID, LayerInsertion, LayerStack, Rectangle, Size, UserID, PREVIEW_SUBLAYER_ID,
};
use crate::protocol::message::*;

use std::convert::TryFrom;
use std::mem;
use std::ops::BitOrAssign;
use std::sync::Arc;
use tracing::{error, info, warn};

pub struct CanvasState {
    layerstack: Arc<LayerStack>,
    history: History,
    brushcache: ClassicBrushCache,
    localfork: LocalFork,
}

/// A structure describing which parts of the canvas were changed
/// and are in need of refresh in the UI.
/// In addition to the usual layerstack area of effect,
/// we're interested in whether layer structure or annotations
/// were changed as well.
/// This is also used to report user cursor movements for display
/// in the UI layer.
pub struct CanvasStateChange {
    pub aoe: AoE,
    pub layers_changed: bool,
    pub annotations_changed: bool,
    pub metadata_changed: bool,
    pub timeline_changed: bool,

    /// This user's cursor moved while doing this change
    pub user: UserID,

    /// The user's target layer
    pub layer: u16,

    /// The user's last cursor position
    pub cursor: (i32, i32),
}

impl CanvasStateChange {
    pub fn nothing() -> Self {
        CanvasStateChange {
            aoe: AoE::Nothing,
            layers_changed: false,
            annotations_changed: false,
            metadata_changed: false,
            timeline_changed: false,
            user: 0,
            layer: 0,
            cursor: (0, 0),
        }
    }

    pub fn everything() -> Self {
        Self {
            aoe: AoE::Resize(0, 0, Size::new(0, 0)),
            layers_changed: true,
            annotations_changed: true,
            metadata_changed: true,
            timeline_changed: true,
            user: 0,
            layer: 0,
            cursor: (0, 0),
        }
    }

    fn layers(aoe: AoE) -> Self {
        Self {
            aoe,
            layers_changed: true,
            ..Self::nothing()
        }
    }

    fn annotations(user: UserID, cursor: (i32, i32)) -> Self {
        CanvasStateChange {
            annotations_changed: true,
            user,
            cursor,
            ..Self::nothing()
        }
    }

    fn metadata() -> Self {
        Self {
            metadata_changed: true,
            ..Self::nothing()
        }
    }

    fn timeline() -> Self {
        Self {
            timeline_changed: true,
            ..Self::nothing()
        }
    }

    fn aoe(aoe: AoE, user: UserID, layer: u16, cursor: (i32, i32)) -> Self {
        Self {
            aoe,
            user,
            layer,
            cursor,
            ..Self::nothing()
        }
    }

    fn compare(s1: &LayerStack, s2: &LayerStack) -> Self {
        Self {
            aoe: s1.compare(s2),
            layers_changed: s1.root().compare_structure(s2.root()),
            annotations_changed: s1.compare_annotations(s2),
            metadata_changed: s1.compare_metadata(s2),
            timeline_changed: s1.compare_timeline(s2),
            user: 0,
            layer: 0,
            cursor: (0, 0),
        }
    }

    pub fn has_any(&self) -> bool {
        match &self.aoe {
            AoE::Nothing => {}
            _ => {
                return true;
            }
        };
        self.layers_changed
            | self.annotations_changed
            | self.metadata_changed
            | self.timeline_changed
    }
}

impl From<AoE> for CanvasStateChange {
    fn from(aoe: AoE) -> Self {
        Self {
            aoe,
            ..Self::nothing()
        }
    }
}

impl BitOrAssign for CanvasStateChange {
    fn bitor_assign(&mut self, rhs: Self) {
        let aoe = std::mem::replace(&mut self.aoe, AoE::Nothing);
        self.aoe = aoe.merge(rhs.aoe);
        self.layers_changed |= rhs.layers_changed;
        self.annotations_changed |= rhs.annotations_changed;
        self.metadata_changed |= rhs.metadata_changed;
        self.timeline_changed |= rhs.timeline_changed;
        if rhs.user > 0 {
            self.user = rhs.user;
            self.layer = rhs.layer;
            self.cursor = rhs.cursor;
        }
    }
}

impl CanvasState {
    pub fn new() -> CanvasState {
        CanvasState {
            layerstack: Arc::new(LayerStack::new(0, 0)),
            history: History::new(),
            brushcache: ClassicBrushCache::new(),
            localfork: LocalFork::new().set_fallbehind(1000),
        }
    }

    pub fn new_with_layerstack(layerstack: LayerStack) -> Self {
        CanvasState {
            layerstack: Arc::new(layerstack),
            history: History::new(),
            brushcache: ClassicBrushCache::new(),
            localfork: LocalFork::new().set_fallbehind(1000),
        }
    }

    pub fn layerstack(&self) -> &LayerStack {
        &self.layerstack
    }

    pub fn arc_layerstack(&self) -> Arc<LayerStack> {
        self.layerstack.clone()
    }

    /// Receive a message from the canonical session history and execute it
    pub fn receive_message(&mut self, msg: &CommandMessage) -> CanvasStateChange {
        self.history.add(msg.clone());

        let retcon = self.localfork.receive_remote_message(msg);

        match retcon {
            RetconAction::Concurrent => self.handle_message(msg),
            RetconAction::AlreadyDone => CanvasStateChange::nothing(),
            RetconAction::Rollback(pos) => {
                info!(
                    "History conflict with {:?}, (local fork tail at {}, history at {}).",
                    msg,
                    pos,
                    self.history.end()
                );
                let replay = self.history.reset_before(pos);

                // If the local fork still exists, move it to the end of the history
                self.localfork.set_seqnum(self.history.end());

                if let Some((savepoint, messages)) = replay {
                    let old_layerstack = mem::replace(&mut self.layerstack, savepoint);
                    let localfork = self.localfork.messages();
                    info!(
                        "Rolling back {} history + {} localfork messages.",
                        messages.len(),
                        localfork.len()
                    );

                    for m in messages.iter().chain(localfork.iter()) {
                        self.handle_message(m);
                    }

                    CanvasStateChange::compare(&old_layerstack, &self.layerstack)
                } else {
                    error!("Retcon failed! No savepoint found before {}", pos);
                    CanvasStateChange::nothing()
                }
            }
        }
    }

    /// Receive a locally generated message that is not yet in the canonical
    /// history and execute it. If an official message conflicts with any of
    /// the locally added messages, the canvas is is reset to an earlier savepoint
    /// and the history straightened out.
    pub fn receive_local_message(&mut self, msg: &CommandMessage) -> CanvasStateChange {
        match msg {
            CommandMessage::UndoPoint(_) => {
                // Try creating a new savepoint before the local fork exists
                self.make_savepoint_if_needed();
            }
            CommandMessage::Undo(_, _) => {
                // Undos have to go through the server
                return CanvasStateChange::nothing();
            }
            _ => (),
        }
        self.localfork.add_local_message(msg, self.history.end());
        self.handle_message(msg)
    }

    /// Apply commands to a preview layer. This is used to preview
    /// shape tools and selection cutouts.
    pub fn apply_preview(
        &mut self,
        layer_id: LayerID,
        msgs: &[CommandMessage],
    ) -> CanvasStateChange {
        if msgs.is_empty() {
            return CanvasStateChange::nothing();
        }

        if let Some(layer) = Arc::make_mut(&mut self.layerstack)
            .root_mut()
            .get_bitmaplayer_mut(layer_id)
        {
            let layer = layer.get_or_create_sublayer(PREVIEW_SUBLAYER_ID);
            let mut aoe = editlayer::clear_layer(layer);

            match msgs.first().unwrap() {
                CommandMessage::DrawDabsClassic(_, m) => {
                    layer.metadata_mut().blendmode =
                        Blendmode::try_from(m.mode).unwrap_or_default();
                    let color = Color::from_argb32(m.color);
                    layer.metadata_mut().opacity = if color.a > 0.0 { color.a } else { 1.0 };
                }
                CommandMessage::DrawDabsPixel(_, m) | CommandMessage::DrawDabsPixelSquare(_, m) => {
                    layer.metadata_mut().blendmode =
                        Blendmode::try_from(m.mode).unwrap_or_default();
                    let color = Color::from_argb32(m.color);
                    layer.metadata_mut().opacity = if color.a > 0.0 { color.a } else { 1.0 };
                }
                CommandMessage::FillRect(_, m) => {
                    layer.metadata_mut().blendmode =
                        Blendmode::try_from(m.mode).unwrap_or_default();
                    layer.metadata_mut().opacity = 1.0;
                }
                CommandMessage::PutImage(_, m) => {
                    layer.metadata_mut().blendmode =
                        Blendmode::try_from(m.mode).unwrap_or_default();
                    layer.metadata_mut().opacity = 1.0;
                }
                _ => (),
            };

            for msg in msgs {
                aoe = aoe.merge(match msg {
                    CommandMessage::DrawDabsClassic(_, m) => {
                        let (a, _) = brushes::drawdabs_classic(layer, 0, m, &mut self.brushcache);
                        a
                    }
                    CommandMessage::DrawDabsPixel(_, m) => {
                        let (a, _) = brushes::drawdabs_pixel(layer, 0, m, false);
                        a
                    }
                    CommandMessage::DrawDabsPixelSquare(_, m) => {
                        let (a, _) = brushes::drawdabs_pixel(layer, 0, m, true);
                        a
                    }
                    CommandMessage::DrawDabsMyPaint(_, m) => {
                        let (a, _) = brushes::drawdabs_mypaint(layer, 0, m);
                        a
                    }
                    CommandMessage::PutImage(_, m) => {
                        if m.w == 0 || m.h == 0 {
                            warn!("Preview PutImage: zero size!");
                            AoE::Nothing
                        } else if m.w > 65535 || m.h > 65535 {
                            warn!("Preview PutImage: oversize image! ({}, {})", m.w, m.h);
                            AoE::Nothing
                        } else if let Some(imagedata) =
                            compression::decompress_image(&m.image, (m.w * m.h) as usize)
                        {
                            editlayer::draw_image8(
                                layer,
                                0,
                                &imagedata,
                                &Rectangle::new(m.x as i32, m.y as i32, m.w as i32, m.h as i32),
                                1.0,
                                Blendmode::Normal,
                            )
                        } else {
                            AoE::Nothing
                        }
                    }
                    CommandMessage::FillRect(_, m) => editlayer::fill_rect(
                        layer,
                        0,
                        &Color::from_argb32(m.color),
                        Blendmode::Normal,
                        &Rectangle::new(m.x as i32, m.y as i32, m.w as i32, m.h as i32),
                    ),
                    m => {
                        warn!("Unhandled preview command {:?}", m);
                        AoE::Nothing
                    }
                });
            }

            aoe.into()
        } else {
            warn!("apply_preview: Layer {} not found!", layer_id);
            CanvasStateChange::nothing()
        }
    }

    /// Remove a preview layer or all preview layers if id is 0
    pub fn remove_preview(&mut self, layer_id: LayerID) -> CanvasStateChange {
        if layer_id == 0 {
            Arc::make_mut(&mut self.layerstack)
                .root_mut()
                .inner_mut()
                .visit_bitmaps_mut(|layer: &mut BitmapLayer| {
                    editlayer::remove_sublayer(layer, PREVIEW_SUBLAYER_ID)
                })
                .into()
        } else if let Some(layer) = Arc::make_mut(&mut self.layerstack)
            .root_mut()
            .get_bitmaplayer_mut(layer_id)
        {
            editlayer::remove_sublayer(layer, PREVIEW_SUBLAYER_ID).into()
        } else {
            warn!("remove_preview: Layer {} not found!", layer_id);
            CanvasStateChange::nothing()
        }
    }

    /// Clean up the state after disconnecting from a remote session
    ///
    /// This does the following:
    ///  * moves the local fork to the mainline history
    ///  * merges all sublayers in case there are any remaining indirect strokes
    pub fn cleanup(&mut self) -> AoE {
        for msg in self.localfork.messages() {
            self.history.add(msg);
        }
        self.localfork.clear();

        Arc::make_mut(&mut self.layerstack)
            .root_mut()
            .inner_mut()
            .visit_bitmaps_mut(editlayer::merge_all_sublayers)
    }

    /// Clear out all undo history
    ///
    /// This is used when soft resetting to synchronize a common
    /// new starting point for session history. Without this, a new
    /// client's session state can get out of sync with the rest if
    /// an undo is used that passes the soft reset snapshot point.
    pub fn truncate_history(&mut self) {
        self.history.truncate();
    }

    fn handle_message(&mut self, msg: &CommandMessage) -> CanvasStateChange {
        use CommandMessage::*;
        match &msg {
            UndoPoint(user) => self.handle_undopoint(*user).into(),
            CanvasResize(_, m) => self.handle_canvas_resize(m).into(),
            LayerCreate(_, m) => self.handle_layer_create(m),
            LayerAttributes(_, m) => self.handle_layer_attributes(m),
            LayerRetitle(_, m) => self.handle_layer_retitle(m),
            LayerOrder(_, m) => self.handle_layer_order(m),
            LayerDelete(_, m) => self.handle_layer_delete(m),
            LayerVisibility(_, m) => self.handle_layer_visibility(m.id, m.visible),
            PutImage(u, m) => self.handle_putimage(*u, m).into(),
            FillRect(user, m) => self.handle_fillrect(*user, m),
            PenUp(user) => self.handle_penup(*user).into(),
            AnnotationCreate(user, m) => self.handle_annotation_create(*user, m),
            AnnotationReshape(user, m) => self.handle_annotation_reshape(*user, m),
            AnnotationEdit(user, m) => self.handle_annotation_edit(*user, m),
            AnnotationDelete(_, id) => self.handle_annotation_delete(*id),
            PutTile(_, m) => self.handle_puttile(m).into(),
            CanvasBackground(_, m) => self.handle_background(m).into(),
            DrawDabsClassic(user, m) => self.handle_drawdabs_classic(*user, m),
            DrawDabsPixel(user, m) => self.handle_drawdabs_pixel(*user, m, false),
            DrawDabsPixelSquare(user, m) => self.handle_drawdabs_pixel(*user, m, true),
            DrawDabsMyPaint(user, m) => self.handle_drawdabs_mypaint(*user, m),
            MoveRect(user, m) => self.handle_moverect(*user, m),
            SetMetadataInt(_, m) => self.handle_metadata_int(m),
            SetMetadataStr(_, _) => CanvasStateChange::nothing(), // no string keys yet
            SetTimelineFrame(_, m) => self.handle_settimeline(m),
            RemoveTimelineFrame(_, m) => self.handle_removetimelineframe(*m),
            Undo(user, m) => self.handle_undo(*user, m),
        }
    }

    fn handle_undopoint(&mut self, _user_id: UserID) -> AoE {
        self.make_savepoint_if_needed();
        // TODO set "has participated" flag
        AoE::Nothing
    }

    fn handle_undo(&mut self, user_id: UserID, msg: &UndoMessage) -> CanvasStateChange {
        // Session operators are allowed to undo/redo other users' work
        let user = if msg.override_user > 0 {
            msg.override_user
        } else {
            user_id
        };

        let replay = if msg.redo {
            self.history.redo(user)
        } else {
            self.history.undo(user)
        };

        if let Some((savepoint, messages)) = replay {
            let old_layerstack = mem::replace(&mut self.layerstack, savepoint);

            self.remove_preview(0);

            // We can move the local fork to the end while we're at it
            self.localfork.set_seqnum(self.history.end());
            let localfork = self.localfork.messages();
            for msg in messages.iter().chain(localfork.iter()) {
                self.handle_message(msg);
            }

            CanvasStateChange::compare(&old_layerstack, &self.layerstack)
        } else {
            CanvasStateChange::nothing()
        }
    }

    fn handle_metadata_int(&mut self, msg: &SetMetadataIntMessage) -> CanvasStateChange {
        let md = Arc::make_mut(&mut self.layerstack).metadata_mut();

        match MetadataInt::try_from(msg.field) {
            Ok(MetadataInt::Dpix) => md.dpix = msg.value,
            Ok(MetadataInt::Dpiy) => md.dpiy = msg.value,
            Ok(MetadataInt::Framerate) => md.framerate = msg.value,
            Ok(MetadataInt::UseTimeline) => md.use_timeline = msg.value != 0,
            Err(_) => {
                warn!("Unknown int metadata key {}", msg.field);
                return CanvasStateChange::nothing();
            }
        }
        CanvasStateChange::metadata()
    }

    fn handle_settimeline(&mut self, msg: &SetTimelineFrameMessage) -> CanvasStateChange {
        let frame = msg.frame as usize;
        if frame > self.layerstack.timeline().frames.len() {
            // New frames can only be added one at a time
            return CanvasStateChange::nothing();
        }

        let f = Frame::try_from(&msg.layers[..]).unwrap_or_default();

        let timeline = Arc::make_mut(&mut self.layerstack).timeline_mut();
        if frame == timeline.frames.len() {
            timeline.frames.push(f);
        } else if msg.insert {
            timeline.frames.insert(frame, f);
        } else {
            timeline.frames[frame] = f;
        }
        CanvasStateChange::timeline()
    }

    fn handle_removetimelineframe(&mut self, frame: u16) -> CanvasStateChange {
        let frame = frame as usize;
        if frame >= self.layerstack.timeline().frames.len() {
            return CanvasStateChange::nothing();
        }

        let timeline = Arc::make_mut(&mut self.layerstack).timeline_mut();
        timeline.frames.remove(frame);

        CanvasStateChange::timeline()
    }

    /// Penup ends indirect strokes by merging the user's sublayers.
    fn handle_penup(&mut self, user_id: UserID) -> AoE {
        // find the ID of the layer that contains the sublayer
        // or 0 if none exists
        fn find_sublayer(group: &GroupLayer, sublayer_id: LayerID) -> LayerID {
            for l in group.iter_layers() {
                match l {
                    Layer::Group(g) => {
                        if find_sublayer(g, sublayer_id) != 0 {
                            return g.metadata().id;
                        }
                    }
                    Layer::Bitmap(b) => {
                        if b.has_sublayer(sublayer_id) {
                            return b.metadata().id;
                        }
                    }
                }
            }
            0
        }

        // Merge the sublayer
        fn merge_sublayer(group: &mut GroupLayer, sublayer_in: LayerID, sublayer_id: LayerID) {
            for l in group.iter_layers_mut() {
                if l.metadata().id == sublayer_in {
                    match Arc::make_mut(l) {
                        Layer::Group(g) => {
                            merge_sublayer(g, find_sublayer(g, sublayer_id), sublayer_id);
                        }
                        Layer::Bitmap(b) => {
                            editlayer::merge_sublayer(b, sublayer_id);
                        }
                    }
                    return;
                }
            }
        }

        // Typically, there is only one sublayer that needs to be merged,
        // so this loop only runs twice.
        // We do the read-only search phase to avoid detaching unrelated layers.
        let sublayer_id = user_id as LayerID;
        loop {
            let sublayer_in = find_sublayer(self.layerstack.root().inner_ref(), sublayer_id);
            if sublayer_in != 0 {
                merge_sublayer(
                    Arc::make_mut(&mut self.layerstack).root_mut().inner_mut(),
                    sublayer_in,
                    sublayer_id,
                );
            } else {
                break;
            }
        }

        // Merging a sublayer shouldn't have any visual effect
        AoE::Nothing
    }

    fn handle_canvas_resize(&mut self, msg: &CanvasResizeMessage) -> AoE {
        let original_size = self.layerstack.root().size();

        if let Some(ls) = self
            .layerstack
            .resized(msg.top, msg.right, msg.bottom, msg.left)
        {
            self.layerstack = Arc::new(ls);
            AoE::Resize(msg.left, msg.top, original_size)
        } else {
            warn!("Invalid resize: {:?}", msg);
            AoE::Nothing
        }
    }

    fn handle_layer_create(&mut self, msg: &LayerCreateMessage) -> CanvasStateChange {
        if msg.id == 0 {
            warn!("Cannot create layer with zero ID");
            return CanvasStateChange::nothing();
        }

        let pos = match (msg.flags & LayerCreateMessage::FLAGS_INTO != 0, msg.target) {
            (true, source) => LayerInsertion::Into(source),
            (false, source) if source > 0 => LayerInsertion::Above(source),
            _ => LayerInsertion::Top,
        };

        let root = Arc::make_mut(&mut self.layerstack).root_mut();

        let new_layer = if msg.source > 0 {
            root.add_layer_copy(msg.id, msg.source, pos)
        } else if msg.flags & LayerCreateMessage::FLAGS_GROUP != 0 {
            root.add_group_layer(msg.id, pos)
        } else {
            root.add_bitmap_layer(msg.id, Color::from_argb32(msg.fill), pos)
        };

        if let Some(new_layer) = new_layer {
            new_layer.metadata_mut().title = msg.name.clone();

            CanvasStateChange::layers(if msg.source > 0 {
                new_layer.nonblank_tilemap().into()
            } else if msg.fill != 0 && msg.flags & LayerCreateMessage::FLAGS_GROUP == 0 {
                AoE::Everything
            } else {
                AoE::Nothing
            })
        } else {
            warn!("Couldn't create layer {:04x}", msg.id);
            CanvasStateChange::nothing()
        }
    }

    fn handle_layer_attributes(&mut self, msg: &LayerAttributesMessage) -> CanvasStateChange {
        if let Some(layer) = Arc::make_mut(&mut self.layerstack)
            .root_mut()
            .get_layer_mut(msg.id as LayerID)
        {
            let aoe = editlayer::change_attributes(
                layer,
                msg.sublayer.into(),
                msg.opacity as f32 / 255.0,
                Blendmode::try_from(msg.blend).unwrap_or(Blendmode::Normal),
                (msg.flags & LayerAttributesMessage::FLAGS_CENSOR) != 0,
                (msg.flags & LayerAttributesMessage::FLAGS_ISOLATED) != 0,
            );

            CanvasStateChange::layers(aoe)
        } else {
            warn!("LayerAttributes: Layer {:04x} not found!", msg.id);
            CanvasStateChange::nothing()
        }
    }

    fn handle_layer_retitle(&mut self, msg: &LayerRetitleMessage) -> CanvasStateChange {
        if let Some(layer) = Arc::make_mut(&mut self.layerstack)
            .root_mut()
            .get_layer_mut(msg.id as LayerID)
        {
            layer.metadata_mut().title = msg.title.clone();
            CanvasStateChange::layers(AoE::Nothing)
        } else {
            warn!("LayerRetitle: Layer {:04x} not found!", msg.id);
            CanvasStateChange::nothing()
        }
    }

    fn handle_layer_order(&mut self, msg: &LayerOrderMessage) -> CanvasStateChange {
        match self.layerstack.reordered(msg.root, &msg.layers) {
            Ok(ls) => {
                self.layerstack = Arc::new(ls);
                CanvasStateChange::layers(AoE::Everything)
            }
            Err(e) => {
                warn!("LayerOrder: Invalid order! ({})", e);
                CanvasStateChange::nothing()
            }
        }
    }

    fn handle_layer_delete(&mut self, msg: &LayerDeleteMessage) -> CanvasStateChange {
        let stack = Arc::make_mut(&mut self.layerstack);
        let id = msg.id as LayerID;

        if msg.id == msg.merge_to {
            warn!("LayerDelete: cannot merge {:04x} to itself!", msg.id);
            return CanvasStateChange::nothing();
        }

        let aoe = if msg.merge_to > 0 {
            if let Some(src) = stack.root().get_layer_rc(id) {
                if let Some(target) = stack.root_mut().get_bitmaplayer_mut(msg.merge_to) {
                    let src_aoe = match src.as_ref() {
                        Layer::Group(g) => editlayer::merge_group(target, g),
                        Layer::Bitmap(b) => editlayer::merge_bitmap(target, b),
                    };
                    // Merging a Normal layer causes no visual change, so we don't need
                    // to update the canvas in that case. Other blend modes potentially
                    // do, so we use the affected area as reported by the merge.
                    if src.metadata().blendmode == Blendmode::Normal {
                        AoE::Nothing
                    } else {
                        src_aoe
                    }
                } else {
                    warn!(
                        "LayerDelete: Cannot merge {:04x} to missing layer {:04x}",
                        id, msg.merge_to
                    );
                    return CanvasStateChange::nothing();
                }
            } else {
                warn!("LayerDelete: Cannot merge missing layer {:04x}", id);
                return CanvasStateChange::nothing();
            }
        } else {
            stack
                .root()
                .get_layer(id)
                .map(|l| l.nonblank_tilemap().into())
                .unwrap_or(AoE::Nothing)
        };

        stack.root_mut().remove_layer(id);
        CanvasStateChange::layers(aoe)
    }

    /// Set a layer's local visibility flag
    pub fn handle_layer_visibility(
        &mut self,
        layer_id: LayerID,
        visible: bool,
    ) -> CanvasStateChange {
        if let Some(layer) = Arc::make_mut(&mut self.layerstack)
            .root_mut()
            .get_layer_mut(layer_id)
        {
            layer.metadata_mut().hidden = !visible;
            CanvasStateChange::layers(layer.nonblank_tilemap().into())
        } else {
            CanvasStateChange::nothing()
        }
    }

    fn handle_annotation_create(
        &mut self,
        user_id: UserID,
        msg: &AnnotationCreateMessage,
    ) -> CanvasStateChange {
        Arc::make_mut(&mut self.layerstack).add_annotation(
            msg.id,
            Rectangle::new(msg.x, msg.y, msg.w.max(1) as i32, msg.h.max(1) as i32),
        );
        CanvasStateChange::annotations(
            user_id,
            (msg.x + msg.w as i32 / 2, msg.y + msg.h as i32 / 2),
        )
    }

    fn handle_annotation_reshape(
        &mut self,
        user_id: UserID,
        msg: &AnnotationReshapeMessage,
    ) -> CanvasStateChange {
        if let Some(a) = Arc::make_mut(&mut self.layerstack).get_annotation_mut(msg.id) {
            a.rect = Rectangle::new(msg.x, msg.y, msg.w.max(1) as i32, msg.h.max(1) as i32);
            CanvasStateChange::annotations(
                user_id,
                (msg.x + msg.w as i32 / 2, msg.y + msg.h as i32 / 2),
            )
        } else {
            CanvasStateChange::nothing()
        }
    }

    fn handle_annotation_edit(
        &mut self,
        user_id: UserID,
        msg: &AnnotationEditMessage,
    ) -> CanvasStateChange {
        if let Some(a) = Arc::make_mut(&mut self.layerstack).get_annotation_mut(msg.id) {
            a.background = Color::from_argb32(msg.bg);
            a.protect = (msg.flags & AnnotationEditMessage::FLAGS_PROTECT) != 0;
            a.valign = match msg.flags & 0x06 {
                AnnotationEditMessage::FLAGS_VALIGN_CENTER => VAlign::Center,
                AnnotationEditMessage::FLAGS_VALIGN_BOTTOM => VAlign::Bottom,
                _ => VAlign::Top,
            };
            a.border = msg.border;
            a.text = msg.text.clone();

            CanvasStateChange::annotations(
                user_id,
                (a.rect.x + a.rect.w / 2, a.rect.y + a.rect.h / 2),
            )
        } else {
            CanvasStateChange::nothing()
        }
    }

    fn handle_annotation_delete(&mut self, id: AnnotationID) -> CanvasStateChange {
        Arc::make_mut(&mut self.layerstack).remove_annotation(id);
        CanvasStateChange::annotations(0, (0, 0))
    }

    fn handle_puttile(&mut self, msg: &PutTileMessage) -> AoE {
        if let Some(layer) = Arc::make_mut(&mut self.layerstack)
            .root_mut()
            .get_bitmaplayer_mut(msg.layer as LayerID)
        {
            if let Some(tile) = compression::decompress_tile(&msg.image, msg.last_touch) {
                return editlayer::put_tile(
                    layer,
                    msg.sublayer.into(),
                    msg.col.into(),
                    msg.row.into(),
                    msg.repeat.into(),
                    &tile,
                );
            }
        } else {
            warn!("PutTile: Layer {:04x} not found!", msg.layer);
        }
        AoE::Nothing
    }

    fn handle_putimage(&mut self, user_id: UserID, msg: &PutImageMessage) -> AoE {
        if let Some(layer) = Arc::make_mut(&mut self.layerstack)
            .root_mut()
            .get_bitmaplayer_mut(msg.layer as LayerID)
        {
            if layer.width() == 0 {
                warn!("Layer has zero size!");
                return AoE::Nothing;
            }
            if msg.w == 0 || msg.h == 0 {
                warn!("PutImage: zero size!");
                return AoE::Nothing;
            }
            if msg.w > 65535 || msg.h > 65535 {
                warn!("PutImage: oversize image! ({}, {})", msg.w, msg.h);
                return AoE::Nothing;
            }
            if let Some(imagedata) =
                compression::decompress_image(&msg.image, (msg.w * msg.h) as usize)
            {
                let mode = Blendmode::try_from(msg.mode).unwrap_or_default();
                let aoe = editlayer::draw_image8(
                    layer,
                    user_id,
                    &imagedata,
                    &Rectangle::new(msg.x as i32, msg.y as i32, msg.w as i32, msg.h as i32),
                    1.0,
                    mode,
                );

                if mode.can_decrease_opacity() {
                    layer.optimize(&aoe);
                }
                return aoe;
            }
        } else {
            warn!("PutImage: Layer {:04x} not found!", msg.layer);
        }
        AoE::Nothing
    }

    fn handle_background(&mut self, pixels: &[u8]) -> AoE {
        if let Some(tile) = compression::decompress_tile(pixels, 0) {
            Arc::make_mut(&mut self.layerstack).background = tile;
            AoE::Everything
        } else {
            AoE::Nothing
        }
    }

    fn handle_fillrect(&mut self, user: UserID, msg: &FillRectMessage) -> CanvasStateChange {
        if msg.w == 0 || msg.h == 0 {
            warn!("FillRect(user {}): zero size rectangle", user);
            return CanvasStateChange::nothing();
        }

        if let Some(layer) = Arc::make_mut(&mut self.layerstack)
            .root_mut()
            .get_bitmaplayer_mut(msg.layer as LayerID)
        {
            let mode = Blendmode::try_from(msg.mode).unwrap_or_default();
            let aoe = editlayer::fill_rect(
                layer,
                user,
                &Color::from_argb32(msg.color),
                mode,
                &Rectangle::new(msg.x as i32, msg.y as i32, msg.w as i32, msg.h as i32),
            );

            if mode.can_decrease_opacity() {
                layer.optimize(&aoe);
            }

            return CanvasStateChange::aoe(
                aoe,
                user,
                msg.layer,
                ((msg.x + msg.w / 2) as i32, (msg.y + msg.h / 2) as i32),
            );
        } else {
            warn!("FillRect: Layer {:04x} not found!", msg.layer);
        }
        CanvasStateChange::nothing()
    }

    fn handle_moverect(&mut self, user: UserID, msg: &MoveRectMessage) -> CanvasStateChange {
        if msg.w <= 0 || msg.h <= 0 {
            warn!("MoveRect(user {}): zero size move rect!", user);
            return CanvasStateChange::nothing();
        }

        let source_rect = Rectangle::new(msg.sx, msg.sy, msg.w, msg.h);

        let mask = if msg.mask.is_empty() {
            None
        } else {
            compression::decompress_image(&msg.mask, (msg.w * msg.h) as usize)
        };

        if let Some(layer) = Arc::make_mut(&mut self.layerstack)
            .root_mut()
            .get_bitmaplayer_mut(msg.layer as LayerID)
        {
            if !source_rect.in_bounds(layer.size()) {
                warn!(
                    "MoveRect(user {}): source rectangle ({:?}) not in canvas bounds!",
                    user, source_rect
                );
                return CanvasStateChange::nothing();
            }

            CanvasStateChange::aoe(
                editlayer::move_rect(layer, user, source_rect, msg.tx, msg.ty, mask.as_deref()),
                user,
                msg.layer,
                (msg.tx + msg.w / 2, msg.ty + msg.h / 2),
            )
        } else {
            warn!("MoveRect: Layer {:04x} not found!", msg.layer);
            CanvasStateChange::nothing()
        }
    }

    fn handle_drawdabs_classic(
        &mut self,
        user: UserID,
        msg: &DrawDabsClassicMessage,
    ) -> CanvasStateChange {
        if let Some(layer) = Arc::make_mut(&mut self.layerstack)
            .root_mut()
            .get_bitmaplayer_mut(msg.layer as LayerID)
        {
            let (aoe, pos) = brushes::drawdabs_classic(layer, user, msg, &mut self.brushcache);
            CanvasStateChange::aoe(aoe, user, msg.layer, pos)
        } else {
            warn!("DrawDabsClassic: Layer {:04x} not found!", msg.layer);
            CanvasStateChange::nothing()
        }
    }

    fn handle_drawdabs_mypaint(
        &mut self,
        user: UserID,
        msg: &DrawDabsMyPaintMessage,
    ) -> CanvasStateChange {
        if let Some(layer) = Arc::make_mut(&mut self.layerstack)
            .root_mut()
            .get_bitmaplayer_mut(msg.layer as LayerID)
        {
            let (aoe, pos) = brushes::drawdabs_mypaint(layer, user, msg);
            CanvasStateChange::aoe(aoe, user, msg.layer, pos)
        } else {
            warn!("DrawDabsMyPaint: Layer {:04x} not found!", msg.layer);
            CanvasStateChange::nothing()
        }
    }

    fn handle_drawdabs_pixel(
        &mut self,
        user: UserID,
        msg: &DrawDabsPixelMessage,
        square: bool,
    ) -> CanvasStateChange {
        if let Some(layer) = Arc::make_mut(&mut self.layerstack)
            .root_mut()
            .get_bitmaplayer_mut(msg.layer as LayerID)
        {
            let (aoe, pos) = brushes::drawdabs_pixel(layer, user, msg, square);
            CanvasStateChange::aoe(aoe, user, msg.layer, pos)
        } else {
            warn!("DrawDabsPixel: Layer {:04x} not found!", msg.layer);
            CanvasStateChange::nothing()
        }
    }

    fn make_savepoint_if_needed(&mut self) {
        // Don't make savepoints while a local fork exists, since
        // there will be stuff on the canvas that is not yet in
        // the mainline session history
        if self.localfork.is_empty() {
            self.history.add_savepoint(self.layerstack.clone());
        }
    }
}

impl Default for CanvasState {
    fn default() -> Self {
        Self::new()
    }
}
