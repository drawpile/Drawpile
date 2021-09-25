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

use super::compression::compress_tile;
use crate::paint::annotation::VAlign;
use crate::paint::{BitmapLayer, GroupLayer, LayerID, LayerStack, LayerTileSet, UserID};
use crate::protocol::aclfilter::{userbits_to_vec, AclFilter};
use crate::protocol::message::*;

use std::convert::TryInto;

/// Create a sequence of commands that reproduces the current state of the canvas
pub fn make_canvas_snapshot(
    user: UserID,
    layerstack: &LayerStack,
    default_layer: LayerID,
    aclfilter: Option<&AclFilter>,
) -> Vec<Message> {
    let mut msgs: Vec<Message> = Vec::new();

    // First, initialize the canvas
    msgs.push(Message::Command(CommandMessage::CanvasResize(
        user,
        CanvasResizeMessage {
            top: 0,
            right: layerstack.root().width() as i32,
            bottom: layerstack.root().height() as i32,
            left: 0,
        },
    )));

    msgs.push(Message::Command(CommandMessage::CanvasBackground(
        user,
        compress_tile(&layerstack.background),
    )));

    if default_layer > 0 {
        msgs.push(Message::ClientMeta(ClientMetaMessage::DefaultLayer(
            user,
            default_layer,
        )));
    }

    // Create layers (recursive)
    create_layers(user, layerstack.root().inner_ref(), aclfilter, &mut msgs);

    // Create annotations
    for a in layerstack.get_annotations().iter() {
        msgs.push(Message::Command(CommandMessage::AnnotationCreate(
            user,
            AnnotationCreateMessage {
                id: a.id,
                x: a.rect.x,
                y: a.rect.y,
                w: a.rect.w as u16,
                h: a.rect.h as u16,
            },
        )));

        msgs.push(Message::Command(CommandMessage::AnnotationEdit(
            user,
            AnnotationEditMessage {
                id: a.id,
                bg: a.background.as_argb32(),
                flags: if a.protect {
                    AnnotationEditMessage::FLAGS_PROTECT
                } else {
                    0
                } | match a.valign {
                    VAlign::Top => 0,
                    VAlign::Center => AnnotationEditMessage::FLAGS_VALIGN_CENTER,
                    VAlign::Bottom => AnnotationEditMessage::FLAGS_VALIGN_BOTTOM,
                },
                border: 0,
                text: a.text.clone(),
            },
        )));
    }

    // ACLs
    if let Some(acl) = aclfilter {
        msgs.push(Message::ClientMeta(ClientMetaMessage::FeatureAccessLevels(
            user,
            vec![
                acl.feature_tiers().put_image.into(),
                acl.feature_tiers().move_rect.into(),
                acl.feature_tiers().resize.into(),
                acl.feature_tiers().background.into(),
                acl.feature_tiers().edit_layers.into(),
                acl.feature_tiers().own_layers.into(),
                acl.feature_tiers().create_annotation.into(),
                acl.feature_tiers().laser.into(),
                acl.feature_tiers().undo.into(),
            ],
        )));

        msgs.push(Message::ClientMeta(ClientMetaMessage::UserACL(
            user,
            userbits_to_vec(&acl.users().locked),
        )));
    }

    msgs
}

fn create_layers(
    user: UserID,
    group: &GroupLayer,
    aclfilter: Option<&AclFilter>,
    msgs: &mut Vec<Message>,
) {
    for layer in group.iter_layers() {
        if let Some(b) = layer.as_bitmap() {
            create_layer(user, b, group.metadata().id.try_into().unwrap(), msgs);
        } else if let Some(g) = layer.as_group() {
            create_group(user, g, group.metadata().id.try_into().unwrap(), msgs);
            create_layers(user, g, aclfilter, msgs);
        } else {
            unreachable!();
        }

        // Set Layer ACLs (if found)
        if let Some(acl) = aclfilter {
            if let Some(layeracl) = acl.layers().get(&layer.id().try_into().unwrap()) {
                msgs.push(Message::ClientMeta(ClientMetaMessage::LayerACL(
                    user,
                    LayerACLMessage {
                        id: layer.id().try_into().unwrap(),
                        flags: layeracl.flags(),
                        exclusive: userbits_to_vec(&layeracl.exclusive),
                    },
                )))
            }
        }
    }
}

fn create_group(user: UserID, layer: &GroupLayer, into: LayerID, msgs: &mut Vec<Message>) {
    let metadata = layer.metadata();
    let layer_id: u16 = metadata.id.try_into().unwrap();

    msgs.push(Message::Command(CommandMessage::LayerCreate(
        user,
        LayerCreateMessage {
            id: layer_id,
            source: 0,
            target: into,
            flags: LayerCreateMessage::FLAGS_GROUP
                | if into > 0 {
                    LayerCreateMessage::FLAGS_INTO
                } else {
                    0
                },
            fill: 0,
            name: metadata.title.clone(),
        },
    )));

    msgs.push(Message::Command(CommandMessage::LayerAttributes(
        user,
        LayerAttributesMessage {
            id: layer_id,
            sublayer: 0,
            flags: if metadata.censored {
                LayerAttributesMessage::FLAGS_CENSOR
            } else {
                0
            } | if metadata.fixed {
                LayerAttributesMessage::FLAGS_FIXED
            } else {
                0
            },
            opacity: (metadata.opacity * 255.0) as u8,
            blend: metadata.blendmode.into(),
        },
    )));
}

fn create_layer(user: UserID, layer: &BitmapLayer, into: LayerID, msgs: &mut Vec<Message>) {
    let tileset = LayerTileSet::from(layer);
    let metadata = layer.metadata();
    let layer_id: u16 = metadata.id.try_into().unwrap();

    msgs.push(Message::Command(CommandMessage::LayerCreate(
        user,
        LayerCreateMessage {
            id: layer_id,
            source: 0,
            target: into,
            flags: if into > 0 {
                LayerCreateMessage::FLAGS_INTO
            } else {
                0
            },
            fill: tileset.background,
            name: metadata.title.clone(),
        },
    )));

    msgs.push(Message::Command(CommandMessage::LayerAttributes(
        user,
        LayerAttributesMessage {
            id: layer_id,
            sublayer: 0,
            flags: if metadata.censored {
                LayerAttributesMessage::FLAGS_CENSOR
            } else {
                0
            } | if metadata.fixed {
                LayerAttributesMessage::FLAGS_FIXED
            } else {
                0
            },
            opacity: (metadata.opacity * 255.0) as u8,
            blend: metadata.blendmode.into(),
        },
    )));

    tileset.to_puttiles(user, layer_id, 0, msgs);

    // Put active sublayer content (if any)
    for sl in layer.iter_sublayers() {
        if sl.metadata().id > 0 && sl.metadata().id < 256 {
            LayerTileSet::from(sl).to_puttiles(user, layer_id, sl.metadata().id as u8, msgs);
        }
    }
}
