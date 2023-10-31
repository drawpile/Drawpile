// SPDX-License-Identifier: GPL-3.0-or-later

mod acl;
mod canvas_state;
mod document_metadata;
mod draw_context;
mod image;
mod key_frame;
mod layer_content;
mod layer_group;
mod layer_list;
mod layer_props;
mod layer_props_list;
mod paint_engine;
mod pixels;
mod player;
mod recorder;
mod tile;
mod timeline;
mod track;
mod types;

pub use acl::AclState;
pub use canvas_state::{
    AttachedCanvasState, AttachedTransientCanvasState, BaseCanvasState, CanvasState,
    DetachedCanvasState, DetachedTransientCanvasState, TransientCanvasState,
};
pub use document_metadata::{
    AttachedDocumentMetadata, AttachedTransientDocumentMetadata, BaseDocumentMetadata,
    DetachedDocumentMetadata, DetachedTransientDocumentMetadata, DocumentMetadata,
    TransientDocumentMetadata,
};
pub use draw_context::DrawContext;
pub use image::Image;
pub use key_frame::{
    AttachedTransientKeyFrame, BaseKeyFrame, DetachedTransientKeyFrame, TransientKeyFrame,
};
pub use layer_content::{
    AttachedLayerContent, AttachedTransientLayerContent, BaseLayerContent, DetachedLayerContent,
    DetachedTransientLayerContent, LayerContent, TransientLayerContent,
};
pub use layer_group::{
    AttachedLayerGroup, AttachedTransientLayerGroup, BaseLayerGroup, DetachedLayerGroup,
    DetachedTransientLayerGroup, LayerGroup, TransientLayerGroup,
};
pub use layer_list::{
    AttachedLayerList, AttachedLayerListEntry, AttachedTransientLayerList, BaseLayerList,
    BaseLayerListEntry, DetachedLayerList, DetachedTransientLayerList, LayerList, LayerListEntry,
    TransientLayerList,
};
pub use layer_props::{
    AttachedLayerProps, AttachedTransientLayerProps, BaseLayerProps, DetachedLayerProps,
    DetachedTransientLayerProps, LayerProps, TransientLayerProps,
};
pub use layer_props_list::{
    AttachedLayerPropsList, AttachedTransientLayerPropsList, BaseLayerPropsList,
    DetachedLayerPropsList, DetachedTransientLayerPropsList, LayerPropsList,
    TransientLayerPropsList,
};
pub use paint_engine::PaintEngine;
pub use pixels::UPixels8;
pub use player::Player;
pub use recorder::Recorder;
pub use tile::{AttachedTile, BaseTile, DetachedTile, Tile};
pub use timeline::{
    AttachedTransientTimeline, BaseTimeline, DetachedTransientTimeline, TransientTimeline,
};
pub use track::{AttachedTransientTrack, BaseTrack, DetachedTransientTrack, TransientTrack};
pub use types::{Attached, CArc, Detached, Persister};
