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

use crate::paint::tile::TILE_SIZEI;
use crate::paint::{Color, LayerID, Rectangle};
use crate::protocol::message::*;

use std::collections::{HashMap, VecDeque};
use tracing::warn;

/// The local fork is a list of messages that have been executed locally
/// but have not yet finished their roundtrip through the server.
/// Typically, people avoid drawing over each other, so most messages from different
/// users arriving at the same-ish time are *concurrent* with the locally executed messages.
/// When this is not the case, the conflict is detected by the local fork and the canvas
/// must be rolled back.
///
/// If the user is not drawing at the moment, the local fork can be discarded at this point,
/// since there are probably more conflicts on the way. However, if the user is still actively
/// drawing, it's better to just move the local fork to the end of the history, because otherwise
/// we'll get into a self-conflict loop as our own messages trickle back.
///
/// Whether the user is drawing or not at the moment is detected by the DrawDab* and PenUp
/// messages.
///
/// Most messages are self-contained and their area of effect is easy to determine. However,
/// when drawing in indirect mode, things get a bit more difficult. In indirect mode,
/// the brush dabs are first drawn on a temporary sublayer, so they are always concurrent.
/// Only when the finished drawing is merged onto the main layer (on PenUp) is there possibility for
/// conflict. When determining the affected area for an indirect dab, the dab bounds are remembered
/// in the `indirect_area` buffer. On PenUp, this buffer will be returned. There is room for improvement here,
/// since the axis aligned bounding rectangle can be excessively large for some indirect strokes. (E.g.
/// a thin diagonal line across the whole canvas.)
pub struct LocalFork {
    /// The content of the local fork
    local: VecDeque<LocalMessage>,
    /// Area of the ongoing (if any) indirect stroke per user. Will be used for PenUp
    indirect_area: HashMap<u8, AffectedArea>,
    /// How many messages is the local fork allowed to fall behind
    fallbehind: u32,
    /// How many messages the local fork has fallen behind
    fallen_behind: u32,
    /// The position in history of the local fork's tail
    seq_num: u32,
    /// Is the local user currently drawing (dabs sent, but penup not yet sent)
    drawing_in_progress: bool,
}

#[derive(PartialEq, Debug)]
pub enum RetconAction {
    /// Message was concurrent with the local fork: OK to apply
    Concurrent,

    /// Message was found at the tail of the local fork and should be skipped
    AlreadyDone,

    /// Message was not concurrent with the local fork: rollback to needed
    /// The canvas should be reset to a savepoint whose seqnum is less or equals
    /// the returned value.
    Rollback(u32),
}

#[derive(PartialEq)]
enum AffectedArea {
    /// User attributes are always concurrent with the local user's operations
    UserAttrs,

    /// Changes to layer attributes (opacity, blendmode, etc.)
    LayerAttrs(LayerID),

    /// Change to an annotation
    Annotation(LayerID),

    /// Layer content changed
    Pixels(LayerID, Rectangle),

    /// Document metadata field change
    DocumentMetadata(u8),

    /// Timeline change
    Timeline(u16),

    /// Fallback
    Everything,
}

struct LocalMessage(CommandMessage, AffectedArea);

impl LocalFork {
    pub fn new() -> LocalFork {
        LocalFork {
            local: VecDeque::new(),
            indirect_area: HashMap::new(),
            fallbehind: 0,
            fallen_behind: 0,
            seq_num: 0,
            drawing_in_progress: false,
        }
    }

    pub fn messages(&self) -> Vec<CommandMessage> {
        self.local.iter().map(|lm| lm.0.clone()).collect()
    }

    /// Set the fallbehind limit.
    ///
    /// If the local form falls behind by this many messages, force a rollback.
    /// Generally, we don't want the local fork to live for too long, as it blocks the
    /// creation of savepoints. A long lived local fork often indicates an error: for
    /// whatever reason a message has been lost. There's a good change the next message
    /// making the roundtrip will not be the expected one, resulting in a rollback. If the
    /// local fork is too far behind, this rollback will be huge.
    pub fn set_fallbehind(mut self, value: u32) -> Self {
        self.fallbehind = value;
        self
    }

    /// Change the sequence number (position in history) of the fork point.
    /// This is used when moving the fork point point during a rollback.
    pub fn set_seqnum(&mut self, value: u32) {
        self.seq_num = value;
    }

    /// Add a message to the local fork
    pub fn add_local_message(&mut self, msg: &CommandMessage, seq_num: u32) {
        if self.local.is_empty() {
            self.seq_num = seq_num;
        } else {
            // The local user's ID shouldn't change
            debug_assert_eq!(self.local.front().unwrap().0.user(), msg.user());
        }

        use CommandMessage::*;
        match msg {
            DrawDabsClassic(_, _) | DrawDabsPixel(_, _) | DrawDabsPixelSquare(_, _) => {
                self.drawing_in_progress = true
            }
            PenUp(_) => self.drawing_in_progress = false,
            _ => (),
        }

        let area = self.area_from_message(msg);
        self.local.push_back(LocalMessage(msg.clone(), area));
    }

    /// Receive a message from the server (the canonical session history).
    /// If it's ours, the corresponding one will be removed from the local fork.
    /// Returns what must be done before processing the message.
    pub fn receive_remote_message(&mut self, msg: &CommandMessage) -> RetconAction {
        if self.is_empty() {
            return RetconAction::Concurrent;
        }

        // Check if this is our message that has finished its roundtrip
        let first = self.local.front().unwrap();
        if msg.user() == first.0.user() {
            if *msg == first.0 {
                self.local.pop_front();
                if self.local.is_empty() {
                    self.fallen_behind = 0;
                }
                return RetconAction::AlreadyDone;
            } else {
                // Unusual, but not (necessarily) an error. This can happen when the layer is locked
                // while drawing or when an operator performs some function on behalf the user.
                warn!(
                    "Local fork out of sync. Discarding {} messages.\nGot: {:?}\nExpected: {:?}",
                    self.local.len(),
                    msg,
                    first.0
                );
                self.clear();
                return RetconAction::Rollback(self.seq_num);
            }
        }

        // Check if we've fallen too much behind
        if self.fallbehind > 0 {
            self.fallen_behind += 1;
            if self.fallen_behind > self.fallbehind {
                warn!("Local fork has fallen too much behind.");
                self.clear();
                return RetconAction::Rollback(self.seq_num);
            }
        }

        // This is some other user's message. Check if it's concurrent with the local fork
        let area = self.area_from_message(msg);
        if self.local.iter().all(|lm| lm.1.is_concurrent_with(&area)) {
            RetconAction::Concurrent
        } else {
            if !self.drawing_in_progress {
                self.clear();
            }

            RetconAction::Rollback(self.seq_num)
        }
    }

    pub fn is_empty(&self) -> bool {
        self.local.is_empty()
    }

    pub fn clear(&mut self) {
        self.local.clear();
        self.fallen_behind = 0;
    }

    /// Get the message's area of effect.
    /// Note! This function is not side effect free! It updates the indirect
    /// area buffer and should be called only once per message!
    fn area_from_message(&mut self, msg: &CommandMessage) -> AffectedArea {
        use CommandMessage::*;
        match msg {
            UndoPoint(_) => AffectedArea::UserAttrs,
            CanvasResize(_, _) => AffectedArea::Everything,
            LayerCreate(_, m) => AffectedArea::LayerAttrs(m.id),
            LayerAttributes(_, m) => AffectedArea::LayerAttrs(m.id),
            LayerRetitle(_, m) => AffectedArea::LayerAttrs(m.id),
            LayerOrder(_, _) => AffectedArea::LayerAttrs(0),
            LayerDelete(_, m) => AffectedArea::LayerAttrs(m.id), // TODO this can affect the layer below as well
            LayerVisibility(_, m) => AffectedArea::LayerAttrs(m.id),
            PutImage(_, m) => AffectedArea::Pixels(m.layer, make_rect(m.x, m.y, m.w, m.h)),
            FillRect(_, m) => AffectedArea::Pixels(m.layer, make_rect(m.x, m.y, m.w, m.h)),
            PenUp(u) => self
                .indirect_area
                .remove(u)
                .unwrap_or(AffectedArea::UserAttrs),
            AnnotationCreate(_, m) => AffectedArea::Annotation(m.id),
            AnnotationReshape(_, m) => AffectedArea::Annotation(m.id),
            AnnotationEdit(_, m) => AffectedArea::Annotation(m.id),
            AnnotationDelete(_, id) => AffectedArea::Annotation(*id),
            PutTile(_, m) => {
                if m.sublayer > 0 {
                    self.update_indirect_area(m.sublayer, m.layer, puttile_rect(m))
                } else {
                    AffectedArea::Pixels(m.layer, puttile_rect(m))
                }
            }
            CanvasBackground(_, _) => AffectedArea::LayerAttrs(0),
            DrawDabsClassic(u, m) => {
                if Color::argb32_alpha(m.color) > 0 {
                    self.update_indirect_area(*u, m.layer, classicdabs_area(m))
                } else {
                    AffectedArea::Pixels(m.layer, classicdabs_area(m))
                }
            }
            DrawDabsPixel(u, m) | DrawDabsPixelSquare(u, m) => {
                if Color::argb32_alpha(m.color) > 0 {
                    self.update_indirect_area(*u, m.layer, pixeldabs_area(m))
                } else {
                    AffectedArea::Pixels(m.layer, pixeldabs_area(m))
                }
            }
            DrawDabsMyPaint(_, m) => AffectedArea::Pixels(m.layer, mypaintdabs_area(m)),
            MoveRect(_, m) => AffectedArea::Pixels(
                m.layer,
                Rectangle::new(m.sx, m.sy, m.w, m.h).union(&Rectangle::new(m.tx, m.ty, m.w, m.h)),
            ),
            Undo(_, _) => AffectedArea::UserAttrs, // These are never put in the local fork
            SetMetadataInt(_, m) => AffectedArea::DocumentMetadata(m.field),
            SetMetadataStr(_, m) => AffectedArea::DocumentMetadata(m.field),
            SetTimelineFrame(_, m) => AffectedArea::Timeline(m.frame),
            RemoveTimelineFrame(_, m) => AffectedArea::Timeline(*m),
        }
    }

    fn update_indirect_area(&mut self, user: u8, layer: u16, bounds: Rectangle) -> AffectedArea {
        let area = match self.indirect_area.get(&user) {
            Some(AffectedArea::Pixels(l, a)) => AffectedArea::Pixels(*l, a.union(&bounds)),
            _ => AffectedArea::Pixels(layer, bounds),
        };
        self.indirect_area.insert(user, area);
        AffectedArea::UserAttrs
    }
}

impl AffectedArea {
    pub fn is_concurrent_with(&self, other: &AffectedArea) -> bool {
        use AffectedArea::*;
        match (self, other) {
            (Everything, _) | (_, Everything) => false,
            (LayerAttrs(a), LayerAttrs(b)) => a != b,
            (Annotation(a), Annotation(b)) => a != b,
            (Pixels(al, ar), Pixels(bl, br)) => al != bl || ar.intersected(br).is_none(),
            _ => true,
        }
    }
}

fn make_rect(x: u32, y: u32, w: u32, h: u32) -> Rectangle {
    Rectangle {
        x: x as i32,
        y: y as i32,
        w: (w as i32).max(1),
        h: (h as i32).max(1),
    }
}

fn puttile_rect(msg: &PutTileMessage) -> Rectangle {
    Rectangle::tile(msg.col as i32, msg.row as i32, TILE_SIZEI) // TODO repeat
}

fn classicdabs_area(dabs: &DrawDabsClassicMessage) -> Rectangle {
    let mut last_x = dabs.x as f32 / 4.0;
    let mut last_y = dabs.y as f32 / 4.0;
    let mut bounds = Rectangle::new(last_x as i32, last_y as i32, 1, 1);
    for dab in dabs.dabs.iter() {
        let x = last_x + dab.x as f32 / 4.0;
        let y = last_y + dab.y as f32 / 4.0;
        let r = dab.size as f32 / 256.0 / 2.0;
        let d = (r * 2.0) as i32 + 1;
        bounds = bounds.union(&Rectangle::new((x - r) as i32, (y - r) as i32, d, d));
        last_x = x;
        last_y = y;
    }
    bounds
}

fn pixeldabs_area(dabs: &DrawDabsPixelMessage) -> Rectangle {
    let mut last_x = dabs.x;
    let mut last_y = dabs.y;
    let mut bounds = Rectangle::new(last_x, last_y, 1, 1);
    for dab in dabs.dabs.iter() {
        let x = last_x + dab.x as i32;
        let y = last_y + dab.y as i32;
        let d = dab.size as i32 + 1;
        let r = d / 2;
        bounds = bounds.union(&Rectangle::new(x - r, y - r, d, d));
        last_x = x;
        last_y = y;
    }
    bounds
}

fn mypaintdabs_area(dabs: &DrawDabsMyPaintMessage) -> Rectangle {
    let mut last_x = dabs.x as f32 / 4.0;
    let mut last_y = dabs.y as f32 / 4.0;
    let mut bounds = Rectangle::new(last_x as i32, last_y as i32, 1, 1);
    for dab in dabs.dabs.iter() {
        let x = last_x + dab.x as f32 / 4.0;
        let y = last_y + dab.y as f32 / 4.0;
        let r = dab.size as f32 / 256.0 / 2.0;
        let d = (r * 2.0) as i32 + 1;
        bounds = bounds.union(&Rectangle::new((x - r) as i32, (y - r) as i32, d, d));
        last_x = x;
        last_y = y;
    }
    bounds
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_concurrent() {
        let mut lf = LocalFork::new();

        let messages = vec![
            CommandMessage::UndoPoint(1),
            dabmsg(1, 0, 0, 10, false),
            CommandMessage::PenUp(1),
        ];

        for (i, m) in messages.iter().enumerate() {
            lf.add_local_message(m, i as u32);
        }

        // Drawing by another user: this should be concurrent
        assert_eq!(
            lf.receive_remote_message(&rectmsg(2, 0, 20, 100, 100)),
            RetconAction::Concurrent
        );

        // Now our messages finish their roundtrip
        for m in messages.iter() {
            assert_eq!(lf.receive_remote_message(m), RetconAction::AlreadyDone);
        }
    }

    #[test]
    fn test_conflict() {
        let mut lf = LocalFork::new();

        let messages = vec![CommandMessage::UndoPoint(1), dabmsg(1, 0, 0, 10, false)];

        for (i, m) in messages.iter().enumerate() {
            lf.add_local_message(m, i as u32);
        }

        // Drawing by another user over the same pixels
        assert_eq!(
            lf.receive_remote_message(&rectmsg(2, 0, 0, 100, 100)),
            RetconAction::Rollback(0)
        );

        // Now our messages finish their roundtrip.
        // Because PenUp was not executed yet, the local fork
        // was not automatically cleared.
        for m in messages.iter() {
            assert_eq!(lf.receive_remote_message(m), RetconAction::AlreadyDone);
        }
    }

    #[test]
    fn test_indirect_concurrent() {
        let mut lf = LocalFork::new();

        let messages = vec![
            CommandMessage::UndoPoint(1),
            dabmsg(1, 0, 0, 10, true),
            CommandMessage::PenUp(1),
        ];

        lf.add_local_message(&messages[0], 1);
        lf.add_local_message(&messages[1], 2);

        // Drawing by another user: this intersects our drawing, but
        // should be concurrent because it's in indirect mode
        assert_eq!(
            lf.receive_remote_message(&rectmsg(2, 0, 0, 100, 100)),
            RetconAction::Concurrent
        );

        assert_eq!(
            lf.receive_remote_message(&messages[0]),
            RetconAction::AlreadyDone
        );
        assert_eq!(
            lf.receive_remote_message(&messages[1]),
            RetconAction::AlreadyDone
        );

        lf.add_local_message(&messages[2], 2);

        // Commit is here: this is the part that can conflict in indirect mode
        assert_eq!(
            lf.receive_remote_message(&messages[2]),
            RetconAction::AlreadyDone
        );
    }

    #[test]
    fn test_indirect_conflict() {
        let mut lf = LocalFork::new();

        let messages = vec![
            CommandMessage::UndoPoint(1),
            dabmsg(1, 0, 0, 10, true),
            CommandMessage::PenUp(1),
        ];

        for (i, m) in messages.iter().enumerate() {
            lf.add_local_message(m, i as u32);
        }

        // This intersects with our drawing even in indirect mode
        // because PenUp was executed locally already
        assert_eq!(
            lf.receive_remote_message(&rectmsg(2, 0, 0, 100, 100)),
            RetconAction::Rollback(0)
        );
    }

    fn dabmsg(user: u8, x: i32, y: i32, d: u8, indirect: bool) -> CommandMessage {
        CommandMessage::DrawDabsPixel(
            user,
            DrawDabsPixelMessage {
                layer: 1,
                x,
                y,
                color: if indirect { 0xff_ffffff } else { 0x00_ffffff },
                mode: 1,
                dabs: vec![PixelDab {
                    x: 0,
                    y: 0,
                    size: d,
                    opacity: 255,
                }],
            },
        )
    }

    fn rectmsg(user: u8, x: u32, y: u32, w: u32, h: u32) -> CommandMessage {
        CommandMessage::FillRect(
            user,
            FillRectMessage {
                layer: 1,
                x,
                y,
                w,
                h,
                mode: 1,
                color: 0xff_ffffff,
            },
        )
    }
}
