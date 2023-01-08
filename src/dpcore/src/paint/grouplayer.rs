// This file is part of Drawpile.
// Copyright (C) 2020-2022 Calle Laakkonen
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

use super::aoe::{AoE, TileMap};
use super::bitmaplayer::BitmapLayer;
use super::blendmode::Blendmode;
use super::color::{Color, ALPHA_CHANNEL, ZERO_PIXEL15};
use super::idgenerator::IDGenerator;
use super::layer::{Layer, LayerMetadata};
use super::rect::Size;
use super::tile::{Tile, TileData, TILE_SIZE};
use super::{LayerID, UserID};

use std::collections::HashMap;
use std::sync::Arc;

#[derive(Clone)]
pub(super) enum NewLayerType {
    /// Make a new solid bitmap layer
    Solid(Color),

    /// Make a copy of an existing layer
    Copy(Arc<Layer>),

    /// Make a new layer group
    Group,
}

pub enum LayerInsertion {
    /// Put the new layer at the top of the root group
    Top,

    /// Put the new layer above this layer (inside the group containing it)
    Above(LayerID),

    /// Put the new layer at the top of the given group
    Into(LayerID),
}

/// A group layer has no pixel data of its own, but contains
/// child layers, which may also be groups.
#[derive(Clone)]
pub struct GroupLayer {
    pub(super) metadata: LayerMetadata,
    width: u32,
    height: u32,
    layers: Vec<Arc<Layer>>,
    routes: LayerRoutes,
}

/// A wrapper for the root of a layer tree.
///
/// This provides functions that must always be executed
/// on the root for invariants to hold.
#[derive(Clone)]
pub struct RootGroup(GroupLayer);

/// Routes to sublayers. Each group holds a list of layer IDs
/// contained by its own sub-groups. This allows fast lookup of
/// layers deep in the group hierarchy without having to do a search.
///
/// The layer route maps the target layer ID to the index of the subgroup
/// in this layer group. We can use direct indices rather than group IDs,
/// since the routing tables are rebuilt on every layer reorder anyway.
#[derive(Clone)]
struct LayerRoutes {
    /// We use a vec rather than a hashmap here, because the number of
    /// route entries is generally very low, so a simple linear search
    /// is likely faster than a whole hashmap.
    /// (this is a guess, needs benchmarking to verify.)
    /// Note: u16 instead of usize because the number of layers can't be
    /// bigger than that anyway, and this way the whole tuple fits into
    /// 4 bytes without padding.
    routes: Vec<(LayerID, u16)>,
}

impl LayerRoutes {
    fn new() -> Self {
        LayerRoutes { routes: Vec::new() }
    }

    /// Return the number of routes
    /// This is the same as the sum number of layers contained by all
    /// of the parent group's subgroups
    fn len(&self) -> usize {
        self.routes.len()
    }

    /// Add a route to a layer within one of the owning layer group's
    /// subgroups.
    fn add(&mut self, target: LayerID, subgroup_index: usize) {
        debug_assert!(!self.routes.iter().any(|r| r.0 == target));
        debug_assert!(subgroup_index <= 0xffff);

        self.routes.push((target, subgroup_index as u16));
    }

    /// Find a route to the the given layer, if it exists in oen of
    /// the owning layer group's subgroups.
    fn get(&self, target: LayerID) -> Option<usize> {
        self.routes
            .iter()
            .find(|r| r.0 == target)
            .map(|r| r.1)
            .map(|id| id as usize)
    }

    fn extend_from(&mut self, group: &GroupLayer, index: usize) {
        for l in &group.layers {
            self.add(l.id(), index);
        }
        for (id, _) in &group.routes.routes {
            self.add(*id, index);
        }
    }
}

impl GroupLayer {
    pub fn new(id: LayerID, width: u32, height: u32) -> Self {
        Self {
            metadata: LayerMetadata {
                id,
                title: String::new(),
                opacity: 1.0,
                hidden: false,
                censored: false,
                blendmode: Blendmode::Normal,
                isolated: true,
            },
            width,
            height,
            layers: Vec::new(),
            routes: LayerRoutes::new(),
        }
    }

    pub fn from_parts(
        metadata: LayerMetadata,
        width: u32,
        height: u32,
        layers: Vec<Arc<Layer>>,
    ) -> Self {
        let mut routes = LayerRoutes::new();
        for (i, l) in layers.iter().map(|l| l.as_ref()).enumerate() {
            if let Layer::Group(g) = l {
                routes.extend_from(g, i);
            }
        }

        Self {
            metadata,
            width,
            height,
            layers,
            routes,
        }
    }

    pub fn width(&self) -> u32 {
        self.width
    }

    pub fn height(&self) -> u32 {
        self.height
    }

    pub fn size(&self) -> Size {
        Size::new(self.width as i32, self.height as i32)
    }

    pub fn metadata(&self) -> &LayerMetadata {
        &self.metadata
    }

    pub fn metadata_mut(&mut self) -> &mut LayerMetadata {
        &mut self.metadata
    }

    /// Recursively find a layer with the given ID and return a reference to it
    pub fn get_layer(&self, id: LayerID) -> Option<&Layer> {
        if let Some(subgroup) = self.routes.get(id) {
            // If layer is found in the routing table, it must be inside a subgroup
            // l must be a group, because only groups are added to the routing table
            return self.layers[subgroup].as_group().unwrap().get_layer(id);
        } else {
            // If layer is not routed, it must be a direct child of this
            // group or not exist at all.
            for l in &self.layers {
                if l.id() == id {
                    return Some(l);
                }
            }
        }
        None
    }

    /// Recursively find a layer with the given ID and return a reference counted copy
    pub fn get_layer_rc(&self, id: LayerID) -> Option<Arc<Layer>> {
        if let Some(subgroup) = self.routes.get(id) {
            // l must be a group, because only groups are added to the routing table
            return self.layers[subgroup].as_group().unwrap().get_layer_rc(id);
        } else {
            for l in &self.layers {
                if l.id() == id {
                    return Some(l.clone());
                }
            }
        }
        None
    }

    /// Recursively find a layer with the given ID and return a mutable reference to it
    pub fn get_layer_mut(&mut self, id: LayerID) -> Option<&mut Layer> {
        if let Some(subgroup) = self.routes.get(id) {
            // l must be a group, because only groups are added to the routing table
            return Arc::make_mut(&mut self.layers[subgroup])
                .as_group_mut()
                .unwrap()
                .get_layer_mut(id);
        } else {
            for l in &mut self.layers {
                if l.id() == id {
                    return Some(Arc::make_mut(l));
                }
            }
        }
        None
    }

    /// Get a reference to a bitmap layer
    pub fn get_bitmaplayer(&self, id: LayerID) -> Option<&BitmapLayer> {
        self.get_layer(id).and_then(|l| l.as_bitmap())
    }

    /// Get a mutable reference to a bitmap layer
    pub fn get_bitmaplayer_mut(&mut self, id: LayerID) -> Option<&mut BitmapLayer> {
        self.get_layer_mut(id).and_then(|l| l.as_bitmap_mut())
    }

    // Recursively visit each layer without mutating them
    pub fn visit_layers<F>(&self, visitor: &mut F)
    where
        F: FnMut(&Arc<Layer>),
    {
        for l in &self.layers {
            visitor(l);
            if let Layer::Group(g) = l.as_ref() {
                g.visit_layers(visitor);
            }
        }
    }

    /// Recursively visit each bitmap layer to make changes
    pub fn visit_bitmaps_mut(&mut self, visitor: fn(&mut BitmapLayer) -> AoE) -> AoE {
        let mut aoe = AoE::Nothing;

        for l in self.iter_layers_mut() {
            aoe = aoe.merge(match Arc::make_mut(l) {
                Layer::Group(g) => g.visit_bitmaps_mut(visitor),
                Layer::Bitmap(b) => visitor(b),
            });
        }

        aoe
    }

    /// Add a new layer and return a mutable reference to it
    ///
    /// The following checks must be made to ensure the layerstack
    /// remains in a valid state:
    ///
    ///  - new layer should have a unique ID
    ///  - when adding to a group, a new route must be added to every parent group
    ///  - when copying a group, the IDs of the copied layers must be made unique
    fn add_layer(
        &mut self,
        id: LayerID,
        new_type: NewLayerType,
        pos: LayerInsertion,
    ) -> Option<&mut Layer> {
        debug_assert!(self.get_layer(id).is_none());

        let insert_idx = match pos {
            LayerInsertion::Top | LayerInsertion::Into(0) => 0,
            LayerInsertion::Above(layer_id) => {
                if let Some(subgroup_idx) = self.routes.get(layer_id) {
                    let newlayer = Arc::make_mut(&mut self.layers[subgroup_idx])
                        .as_group_mut()
                        .unwrap()
                        .add_layer(id, new_type, pos);
                    if newlayer.is_some() {
                        self.routes.add(id, subgroup_idx);
                    }
                    return newlayer;
                } else {
                    self.layers
                        .iter()
                        .position(|l| l.id() == layer_id)
                        .unwrap_or(0)
                }
            }
            LayerInsertion::Into(group_id) => {
                if self.metadata.id == group_id {
                    // Target group is this group
                    0
                } else if let Some(subgroup_idx) = self.routes.get(group_id) {
                    // Target group is inside one of this group's subgroups
                    let newlayer = Arc::make_mut(&mut self.layers[subgroup_idx])
                        .as_group_mut()
                        .unwrap()
                        .add_layer(id, new_type, pos);
                    if newlayer.is_some() {
                        self.routes.add(id, subgroup_idx);
                    }
                    return newlayer;
                } else if let Some(idx) = self.layers.iter().position(|l| l.id() == group_id) {
                    // Target group is this group's subgroup
                    let newlayer = Arc::make_mut(&mut self.layers[idx])
                        .as_group_mut()
                        .and_then(|g| g.add_layer(id, new_type, pos));

                    if newlayer.is_some() {
                        self.routes.add(id, idx);
                    }

                    return newlayer;
                } else {
                    // Target group does not exists
                    return None;
                }
            }
        };

        let new_layer = match new_type {
            NewLayerType::Solid(c) => Arc::new(Layer::Bitmap(BitmapLayer::new(
                id,
                self.width,
                self.height,
                Tile::new(&c, 0),
            ))),
            NewLayerType::Copy(mut source) => {
                Arc::make_mut(&mut source).metadata_mut().id = id;
                source
            }
            NewLayerType::Group => {
                Arc::new(Layer::Group(GroupLayer::new(id, self.width, self.height)))
            }
        };

        self.layers.insert(insert_idx, new_layer);

        // Layer order may have changed so we need to rebuild the routes
        let mut routes = LayerRoutes::new();
        for (i, l) in self.layers.iter().enumerate() {
            if let Layer::Group(g) = l.as_ref() {
                routes.extend_from(g, i);
            }
        }
        self.routes = routes;

        Some(Arc::make_mut(&mut self.layers[insert_idx]))
    }

    /// Remove a layer from this group or one of its subgroups
    ///
    /// Note: this must be called on the root group to ensure
    /// layer routes are properly removed
    fn remove_layer(&mut self, id: LayerID) {
        if let Some(subgroup) = self.routes.get(id) {
            // l must be a group, because only groups are added to the routing table
            Arc::make_mut(&mut self.layers[subgroup])
                .as_group_mut()
                .unwrap()
                .remove_layer(id);
        } else {
            self.layers.retain(|l| l.id() != id);
        }

        // rebuild routes
        let mut routes = LayerRoutes::new();
        for (i, l) in self.layers.iter().enumerate() {
            if let Layer::Group(g) = l.as_ref() {
                routes.extend_from(g, i);
            }
        }
        self.routes = routes;
    }

    /// Make a copy of this group with a new ID and the IDs
    /// of all sublayers also updated.
    ///
    /// Layer IDs are generated with the same user prefix as
    /// given for this one.
    ///
    /// Returns None if there are not enough unique LayerIDs
    /// remaining.
    /// On success, returns the new group layer and the last used ID.
    fn make_unique(&self, prefix: u16, ids: &mut IDGenerator) -> Option<GroupLayer> {
        let mut metadata = self.metadata.clone();
        metadata.id = prefix | ids.take_next()? as u16;

        let mut layers = Vec::with_capacity(self.layers.len());
        let mut routes = LayerRoutes::new();

        for l in &self.layers {
            match l.as_ref() {
                Layer::Group(g) => {
                    let ng = g.make_unique(prefix, ids)?;
                    routes.extend_from(&ng, layers.len());
                    layers.push(Arc::new(Layer::Group(ng)));
                }
                _ => {
                    let mut layer = l.clone();
                    Arc::make_mut(&mut layer).metadata_mut().id = prefix | ids.take_next()? as u16;
                    layers.push(layer);
                }
            }
        }

        Some(GroupLayer {
            metadata,
            layers,
            routes,
            width: self.width,
            height: self.height,
        })
    }

    /// Compare the shapes of the layer trees
    ///
    /// The shape includes the number of layers and the metadata
    /// of the layers
    ///
    /// Returns true if the shapes differ
    pub fn compare_structure(&self, other: &GroupLayer) -> bool {
        if self.layers.len() != other.layers.len() {
            return true;
        }

        for (a, b) in self.layers.iter().zip(other.layers.iter()) {
            if !Arc::ptr_eq(a, b) && a.metadata() != b.metadata() {
                return true;
            }
            match (a.as_ref(), b.as_ref()) {
                (Layer::Group(l1), Layer::Group(l2)) => {
                    if l1.compare_structure(l2) {
                        return true;
                    }
                }
                (Layer::Bitmap(_), Layer::Bitmap(_)) => (),
                _ => {
                    return true;
                }
            }
        }

        false
    }

    /// Compare this layer stack group the other and return an Area Of Effect.
    /// This is used when the entire layer stack is replaced, e.g. when
    /// resetting to an earlier state as a part of an undo operation.
    /// For performance reasons, shallow comparison is used.
    pub fn compare(&self, other: &Self) -> AoE {
        if self.width != other.width || self.height != other.height {
            return AoE::Resize(0, 0, self.size());
        }

        if self.layers.len() != other.layers.len() {
            return AoE::Everything;
        }

        // TODO short circuit if AoE::Everything encountered
        self.layers
            .iter()
            .zip(other.layers.iter())
            .fold(AoE::Nothing, |aoe, (a, b)| aoe.merge(a.compare(b)))
    }

    pub fn flatten_tile(
        &self,
        destination: &mut TileData,
        i: u32,
        j: u32,
        parent_opacity: f32,
        censor: bool,
        highlight_id: UserID,
    ) {
        let opacity = parent_opacity * self.metadata.opacity;
        if self.metadata.hidden || opacity < 1.0 / 256.0 {
            return;
        }

        let flatten = |dest: &mut TileData| {
            for l in self.layers.iter().rev() {
                l.flatten_tile(dest, i, j, opacity, censor, highlight_id);
            }
        };

        if self.metadata.isolated {
            let mut tmp = TileData::new(ZERO_PIXEL15, 0);
            flatten(&mut tmp);
            destination.merge_data(&tmp, 1.0, self.metadata.blendmode);
        } else {
            // In non-isolated mode, the layer group itself has no blending
            // mode, the layers are drawn as thought they belonged directly
            // to the parent group.
            flatten(destination);
        }
    }

    /// Return a resized copy of this group
    /// Returns None if the resize would make the group zero sized
    pub fn resized(&self, top: i32, right: i32, bottom: i32, left: i32) -> Option<Self> {
        let new_width = left + self.width as i32 + right;
        let new_height = top + self.height as i32 + bottom;
        if new_width <= 0 || new_height <= 0 {
            return None;
        }

        Some(Self {
            metadata: self.metadata.clone(),
            width: new_width as u32,
            height: new_height as u32,
            layers: self
                .layers
                .iter()
                .map(|l| Arc::new(l.resized(top, right, bottom, left).unwrap()))
                .collect(),
            routes: self.routes.clone(),
        })
    }

    /// Get the layer vector
    /// Note: you typically want to use iter_layers() instead
    pub fn layervec(&self) -> &Vec<Arc<Layer>> {
        &self.layers
    }

    pub fn layer_at(&self, index: usize) -> &Layer {
        &self.layers[index]
    }

    /// Return an interator to the direct children of this group
    pub fn iter_layers(&self) -> impl Iterator + DoubleEndedIterator<Item = &Layer> {
        self.layers.iter().map(|l| l.as_ref())
    }

    /// Return a mutable iterator to the direct children of this group
    pub fn iter_layers_mut(
        &mut self,
    ) -> impl Iterator + DoubleEndedIterator<Item = &mut Arc<Layer>> {
        self.layers.iter_mut()
    }

    /// Return the number of direct children in this group
    pub fn layer_count(&self) -> usize {
        self.layers.len()
    }

    /// Return the total number of layers contained by this group
    pub fn total_layer_count(&self) -> usize {
        self.layers.len() + self.routes.len()
    }

    /// Find the topmost layer with a non-transparent pixel at the given point
    ///
    /// Returns 0 if no layer was found
    pub fn pick_layer(&self, x: i32, y: i32) -> LayerID {
        if x < 0 || y < 0 || x as u32 >= self.width || y as u32 >= self.height {
            return 0;
        }

        for layer in self.layers.iter() {
            if layer.is_visible() {
                match layer.as_ref() {
                    Layer::Group(g) => {
                        let id = g.pick_layer(x, y);
                        if id > 0 {
                            return id;
                        }
                    }
                    Layer::Bitmap(b) => {
                        if b.pixel_at(x as u32, y as u32)[ALPHA_CHANNEL] > 0 {
                            return layer.metadata().id;
                        }
                    }
                }
            }
        }

        0
    }

    pub fn last_edited_by(&self, x: i32, y: i32) -> UserID {
        if x < 0 || y < 0 || x as u32 >= self.width || y as u32 >= self.height {
            return 0;
        }

        for layer in self.layers.iter() {
            if layer.is_visible() {
                match layer.as_ref() {
                    Layer::Group(g) => {
                        let id = g.last_edited_by(x, y);
                        if id > 0 {
                            return id;
                        }
                    }
                    Layer::Bitmap(b) => {
                        if let Tile::Bitmap(td) = b.tile(x as u32 / TILE_SIZE, y as u32 / TILE_SIZE)
                        {
                            return td.last_touched_by;
                        }
                    }
                }
            }
        }
        0
    }

    fn reordered_inner<'a>(
        &self,
        mut expecting: i32,
        new_order: &'a [LayerID],
        all_layers: &mut HashMap<LayerID, Arc<Layer>>,
    ) -> Result<(GroupLayer, &'a [LayerID]), &'static str> {
        // The tree is encoded as a list of (child count, layer ID) pairs.
        // For example: [
        //    3, 0x0001,
        //      2, 0x0201,
        //        0, 0x0202,
        //        0, 0x0203,
        //      0, 0x0101,
        //      1, 0x0102,
        //        0, 0x0301
        // ]
        // The above example has 7 layers total, 3 of which are group layers,
        // and the root group containing just one item, which is a group layer.

        let mut layers: Vec<Arc<Layer>> = if expecting > 0 {
            Vec::with_capacity(expecting as usize)
        } else {
            Vec::new()
        };
        let mut order_slice = new_order;
        let mut routes = LayerRoutes::new();

        while !order_slice.is_empty() && expecting != 0 {
            let layer = all_layers.remove(&order_slice[1]).ok_or("missing layer")?;

            if order_slice[0] == 0 {
                // A leaf node
                layers.push(match layer.as_ref() {
                    Layer::Group(g) => Arc::new(Layer::Group(GroupLayer {
                        metadata: g.metadata.clone(),
                        width: g.width,
                        height: g.height,
                        layers: Vec::new(),
                        routes: LayerRoutes::new(),
                    })),
                    _ => layer,
                });
                order_slice = &order_slice[2..];
            } else {
                // A subgroup
                let group = layer.as_group().ok_or("non-group used as group")?;

                let (reordered, new_slice) =
                    group.reordered_inner(order_slice[0] as i32, &order_slice[2..], all_layers)?;

                routes.extend_from(&reordered, layers.len());
                layers.push(Arc::new(Layer::Group(reordered)));
                order_slice = new_slice;
            }

            expecting -= 1;
        }

        Ok((
            GroupLayer {
                metadata: self.metadata.clone(),
                width: self.width,
                height: self.height,
                layers,
                routes,
            },
            order_slice,
        ))
    }

    /// Return a new group whose layers have been reordered
    /// The new order should contain just the layers of this group
    /// and its descendents.
    fn reordered(&self, new_order: &[LayerID]) -> Result<Self, &'static str> {
        // A valid layer list consists of pairs of numbers
        if new_order.is_empty() {
            return Err("empty ordering");
        }
        if new_order.len() % 2 != 0 {
            return Err("ordering length is not even");
        }

        // First we gather up a flattened map of all the layers
        let mut layermap: HashMap<LayerID, Arc<Layer>> = HashMap::new();
        self.visit_layers(&mut |layer| {
            layermap.insert(layer.id(), layer.clone());
        });

        // Recursively build the new group tree
        let (reordered, _) = self.reordered_inner(-1, new_order, &mut layermap)?;

        // If the reordering command used every layer exactly once,
        // the map should be empty again.
        if !layermap.is_empty() {
            return Err("not all layers were included");
        }

        Ok(reordered)
    }

    pub fn nonblank_tilemap(&self) -> TileMap {
        let mut tm = TileMap::new(self.width, self.height);
        for l in &self.layers {
            tm.tiles |= l.nonblank_tilemap().tiles;
        }
        tm
    }
}

impl RootGroup {
    pub fn new(width: u32, height: u32) -> Self {
        Self(GroupLayer::new(0, width, height))
    }

    /// Unwrap the group
    pub fn inner(self) -> GroupLayer {
        self.0
    }

    pub fn inner_ref(&self) -> &GroupLayer {
        &self.0
    }

    pub fn inner_mut(&mut self) -> &mut GroupLayer {
        &mut self.0
    }

    /// Add a new bitmap layer filled with a solid color and
    /// return a mutable reference to it
    ///
    /// If a layer with the given ID exists already, None will be returned.
    pub fn add_bitmap_layer(
        &mut self,
        id: LayerID,
        fill: Color,
        pos: LayerInsertion,
    ) -> Option<&mut Layer> {
        // Layer ID must be unique
        if self.0.get_layer(id).is_some() {
            return None;
        }

        self.0.add_layer(id, NewLayerType::Solid(fill), pos)
    }

    /// Add a new group and return a mutable reference to it
    ///
    /// If a layer with the given ID exists already, None will be returned.
    pub fn add_group_layer(&mut self, id: LayerID, pos: LayerInsertion) -> Option<&mut Layer> {
        // Layer ID must be unique
        if self.0.get_layer(id).is_some() {
            return None;
        }

        self.0.add_layer(id, NewLayerType::Group, pos)
    }

    /// Make a copy of an existing layer
    ///
    /// If a layer with the given ID exists already, None will be returned.
    ///
    /// When copying a group, new IDs will be generated for each layer in the
    /// group and subgroups. The layer numbering will be restricted to the
    /// same prefix as given to the new group and will begin with the ID
    /// of the group, so that group 0x0304's layers will be numbered 0x0305,
    /// 0x0306 and so on. Existing layer numbers are taken in account when
    /// numbering. If there are not enough available layer IDs, None will
    /// be returned.
    pub fn add_layer_copy(
        &mut self,
        id: LayerID,
        source: LayerID,
        pos: LayerInsertion,
    ) -> Option<&mut Layer> {
        // Layer ID must be unique
        if self.0.get_layer(id).is_some() {
            return None;
        }

        let mut source = self.0.get_layer_rc(source)?;

        if let Some(g) = source.as_group() {
            let mut existing_layers: Vec<u8> = Vec::new();
            self.0.visit_layers(&mut |layer| {
                let layer_id = layer.id();
                if layer_id & 0xff00 == id & 0xff00 {
                    existing_layers.push((layer_id & 0xff) as u8);
                }
            });
            let mut idgen = IDGenerator::new((id & 0xff) as u8, existing_layers);
            source = Arc::new(Layer::Group(g.make_unique(id & 0xff00, &mut idgen)?));
        } else {
            Arc::make_mut(&mut source).metadata_mut().id = id;
        }

        self.0.add_layer(id, NewLayerType::Copy(source), pos)
    }

    /// Find the index of the top-level layer or group that contains this frame
    pub fn find_root_index_by_id(&self, layer_id: LayerID) -> Option<usize> {
        self.0.routes.get(layer_id).or_else(|| {
            for (i, l) in self.iter_layers().enumerate() {
                if l.metadata().id == layer_id {
                    return Some(i);
                }
            }
            None
        })
    }

    /// Return a new group with layers under root_group reordered.
    ///
    /// root_group 0 refers to the whole layer tree.
    pub fn reordered(
        &self,
        root_group: LayerID,
        new_order: &[LayerID],
    ) -> Result<Self, &'static str> {
        if root_group == 0 {
            return Ok(Self(self.0.reordered(new_order)?));
        }

        let mut root = self.clone();

        let group = root
            .get_layer_mut(root_group)
            .and_then(|g| g.as_group_mut())
            .ok_or("root group not found")?;

        *group = group.reordered(new_order)?;

        Ok(root)
    }

    pub fn width(&self) -> u32 {
        self.0.width()
    }

    pub fn height(&self) -> u32 {
        self.0.height()
    }

    pub fn size(&self) -> Size {
        self.0.size()
    }

    pub fn get_layer(&self, id: LayerID) -> Option<&Layer> {
        self.0.get_layer(id)
    }

    pub fn get_layer_rc(&self, id: LayerID) -> Option<Arc<Layer>> {
        self.0.get_layer_rc(id)
    }

    pub fn get_layer_mut(&mut self, id: LayerID) -> Option<&mut Layer> {
        self.0.get_layer_mut(id)
    }

    pub fn get_bitmaplayer(&self, id: LayerID) -> Option<&BitmapLayer> {
        self.0.get_bitmaplayer(id)
    }

    pub fn get_bitmaplayer_mut(&mut self, id: LayerID) -> Option<&mut BitmapLayer> {
        self.0.get_bitmaplayer_mut(id)
    }

    pub fn remove_layer(&mut self, id: LayerID) {
        // this can only be called safely via the root group
        self.0.remove_layer(id);
    }

    pub fn compare_structure(&self, other: &Self) -> bool {
        self.0.compare_structure(&other.0)
    }

    pub fn compare(&self, other: &Self) -> AoE {
        self.0.compare(&other.0)
    }

    pub fn flatten_tile(
        &self,
        destination: &mut TileData,
        i: u32,
        j: u32,
        parent_opacity: f32,
        censor: bool,
        highlight_id: UserID,
    ) {
        self.0
            .flatten_tile(destination, i, j, parent_opacity, censor, highlight_id)
    }

    pub fn resized(&self, top: i32, right: i32, bottom: i32, left: i32) -> Option<Self> {
        Some(Self(self.0.resized(top, right, bottom, left)?))
    }

    pub fn layer_at(&self, index: usize) -> &Layer {
        self.0.layer_at(index)
    }

    pub fn iter_layers(&self) -> impl Iterator + DoubleEndedIterator<Item = &Layer> {
        self.0.iter_layers()
    }

    pub fn iter_layers_mut(
        &mut self,
    ) -> impl Iterator + DoubleEndedIterator<Item = &mut Arc<Layer>> {
        self.0.iter_layers_mut()
    }

    pub fn layer_count(&self) -> usize {
        self.0.layer_count()
    }

    pub fn pick_layer(&self, x: i32, y: i32) -> LayerID {
        self.0.pick_layer(x, y)
    }

    pub fn last_edited_by(&self, x: i32, y: i32) -> UserID {
        self.0.last_edited_by(x, y)
    }

    pub fn nonblank_tilemap(&self) -> TileMap {
        self.0.nonblank_tilemap()
    }
}

impl From<GroupLayer> for RootGroup {
    fn from(root: GroupLayer) -> Self {
        Self(root)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_layer_addition() {
        let mut root = RootGroup::new(256, 256);
        assert!(root
            .add_bitmap_layer(1, Color::TRANSPARENT, LayerInsertion::Top)
            .is_some());

        // Adding a layer with an existing ID does nothing
        assert!(root
            .add_bitmap_layer(1, Color::TRANSPARENT, LayerInsertion::Top)
            .is_none());

        // One more layer on top
        assert!(root
            .add_bitmap_layer(2, Color::rgb8(255, 0, 0), LayerInsertion::Top)
            .is_some());

        // Duplicate layer on top
        assert!(root.add_layer_copy(3, 1, LayerInsertion::Top).is_some());

        // Insert layer above the bottom-most
        assert!(root
            .add_bitmap_layer(4, Color::rgb8(0, 255, 0), LayerInsertion::Above(1))
            .is_some());

        // Insert layer above the topmost
        assert!(root
            .add_bitmap_layer(6, Color::rgb8(0, 0, 255), LayerInsertion::Above(3))
            .is_some());

        assert_eq!(root.layer_count(), 5);
        assert!(root.get_layer(0).is_none());
        assert_eq!(root.0.layers[4].metadata().id, 1);
        assert_eq!(root.0.layers[3].metadata().id, 4);
        assert_eq!(root.0.layers[2].metadata().id, 2);
        assert_eq!(root.0.layers[1].metadata().id, 3);
        assert_eq!(root.0.layers[0].metadata().id, 6);
    }

    #[test]
    fn test_group_adding() {
        let mut group = GroupLayer::new(0, 256, 256);

        // Add a regular layer
        assert!(group
            .add_layer(
                1,
                NewLayerType::Solid(Color::TRANSPARENT),
                LayerInsertion::Top
            )
            .is_some());

        // Add a group layer layer
        assert!(group
            .add_layer(2, NewLayerType::Group, LayerInsertion::Top)
            .is_some());

        // Add another regular layer
        assert!(group
            .add_layer(
                3,
                NewLayerType::Solid(Color::TRANSPARENT),
                LayerInsertion::Top
            )
            .is_some());

        // Add a layer to the group
        assert!(group
            .add_layer(
                20,
                NewLayerType::Solid(Color::TRANSPARENT),
                LayerInsertion::Into(2)
            )
            .is_some());

        // Adding above a layer in a group
        assert!(group
            .add_layer(
                21,
                NewLayerType::Solid(Color::TRANSPARENT),
                LayerInsertion::Above(20)
            )
            .is_some());

        // Adding above a layer in the root
        assert!(group
            .add_layer(
                10,
                NewLayerType::Solid(Color::TRANSPARENT),
                LayerInsertion::Above(2)
            )
            .is_some());

        // Adding to a non-group layer shouldn't work
        assert!(group
            .add_layer(
                90,
                NewLayerType::Solid(Color::TRANSPARENT),
                LayerInsertion::Into(1)
            )
            .is_none());

        // Adding to a non-existent group shouldn't work
        assert!(group
            .add_layer(
                91,
                NewLayerType::Solid(Color::TRANSPARENT),
                LayerInsertion::Into(999)
            )
            .is_none());

        assert_eq!(group.layer_count(), 4);
        assert_eq!(group.total_layer_count(), 6);
        assert!(matches!(group.layers[3].as_ref(), Layer::Bitmap(_)));
        assert!(matches!(group.layers[2].as_ref(), Layer::Group(_)));
        assert!(matches!(group.layers[1].as_ref(), Layer::Bitmap(_)));
        assert!(matches!(group.layers[0].as_ref(), Layer::Bitmap(_)));

        assert_eq!(group.layers[3].id(), 1);
        assert_eq!(group.layers[2].id(), 2);
        assert_eq!(group.layers[1].id(), 10);
        assert_eq!(group.layers[0].id(), 3);

        let g2 = group.get_layer(2).unwrap().as_group().unwrap();
        assert_eq!(g2.layer_count(), 2);
        assert_eq!(g2.layers[1].id(), 20);
        assert_eq!(g2.layers[0].id(), 21);

        assert_eq!(group.get_layer(20).unwrap().id(), 20);
    }

    #[test]
    fn test_group_copying() {
        let mut root = RootGroup::new(128, 128);

        assert!(root.add_group_layer(0x0101, LayerInsertion::Top).is_some());

        // And a sublayer
        assert!(root
            .add_bitmap_layer(0x0201, Color::TRANSPARENT, LayerInsertion::Into(0x0101))
            .is_some());

        // Add a copy of the group
        assert!(root
            .add_layer_copy(0x0102, 0x0101, LayerInsertion::Top)
            .is_some());

        // Now we should have:
        // 0x0101 - group
        //  ↳ 0x0201 - layer
        // 0x0102 - group copy
        //  ↳ 0x0103 - layer copy

        assert_eq!(root.layer_count(), 2);
        assert_eq!(root.0.layers[1].id(), 0x0101);
        assert_eq!(root.0.layers[0].id(), 0x0102);

        // original group
        let og = root.0.layers[1].as_group().unwrap();
        assert_eq!(og.layer_count(), 1);
        assert_eq!(og.layers[0].id(), 0x0201);

        // copied group
        let cg = root.0.layers[0].as_group().unwrap();
        assert_eq!(cg.layer_count(), 1);
        assert_eq!(cg.layers[0].id(), 0x0103);

        // routes set up correctly
        assert!(root.get_layer(0x0103).is_some());
    }

    #[test]
    fn test_layer_removal() {
        let mut group = GroupLayer::new(0, 256, 256);
        group.add_layer(
            1,
            NewLayerType::Solid(Color::TRANSPARENT),
            LayerInsertion::Top,
        );
        group.add_layer(
            2,
            NewLayerType::Solid(Color::TRANSPARENT),
            LayerInsertion::Top,
        );
        group.add_layer(
            3,
            NewLayerType::Solid(Color::TRANSPARENT),
            LayerInsertion::Top,
        );

        assert_eq!(group.layers.len(), 3);
        group.remove_layer(2);
        assert_eq!(group.layers.len(), 2);
        assert!(group.get_layer(1).is_some());
        assert!(group.get_layer(2).is_none());
        assert!(group.get_layer(3).is_some());
    }

    #[test]
    fn test_group_removal() {
        let mut root = RootGroup::new(64, 64);
        root.add_group_layer(1, LayerInsertion::Top);
        root.add_bitmap_layer(2, Color::TRANSPARENT, LayerInsertion::Into(1));
        root.add_bitmap_layer(3, Color::TRANSPARENT, LayerInsertion::Top);
        root.add_bitmap_layer(4, Color::TRANSPARENT, LayerInsertion::Top);

        // Remove the topmost layer. Layer routes should still work
        root.remove_layer(4);
        assert!(root.get_layer(4).is_none());
        assert!(root.get_layer(1).is_some());
        assert!(root.get_layer(2).is_some());
        assert!(root.get_layer(3).is_some());

        // Remove the group (and its contents)
        root.remove_layer(1);
        assert!(root.get_layer(1).is_none());
        assert!(root.get_layer(2).is_none());
        assert!(root.get_layer(3).is_some());
    }

    #[test]
    fn test_subgroup_removal() {
        let mut root = RootGroup::new(64, 64);
        root.add_group_layer(1, LayerInsertion::Top);
        root.add_group_layer(2, LayerInsertion::Into(1));
        root.add_group_layer(3, LayerInsertion::Into(2));
        root.add_bitmap_layer(4, Color::TRANSPARENT, LayerInsertion::Into(3));
        root.add_bitmap_layer(5, Color::TRANSPARENT, LayerInsertion::Into(3));

        root.remove_layer(3);

        assert_eq!(root.0.routes.len(), 1);

        assert!(root.get_layer(1).is_some());
        assert!(root.get_layer(2).is_some());
        assert!(root.get_layer(3).is_none());
        assert!(root.get_layer(4).is_none());
        assert!(root.get_layer(5).is_none());
    }

    #[test]
    fn test_layer_reordering() {
        let mut root = RootGroup::new(64, 64);

        assert!(root
            .add_bitmap_layer(1, Color::TRANSPARENT, LayerInsertion::Top)
            .is_some());
        assert!(root
            .add_bitmap_layer(2, Color::TRANSPARENT, LayerInsertion::Top)
            .is_some());
        assert!(root
            .add_bitmap_layer(3, Color::TRANSPARENT, LayerInsertion::Top)
            .is_some());
        assert!(root
            .add_bitmap_layer(4, Color::TRANSPARENT, LayerInsertion::Top)
            .is_some());
        assert!(root.add_group_layer(5, LayerInsertion::Top).is_some());
        assert!(root
            .add_bitmap_layer(6, Color::TRANSPARENT, LayerInsertion::Into(5))
            .is_some());
        assert!(root.add_group_layer(7, LayerInsertion::Top).is_some());

        assert_eq!(root.0.layers.len(), 6);
        assert_eq!(root.0.layers[5].id(), 1);
        assert_eq!(root.0.layers[4].id(), 2);
        assert_eq!(root.0.layers[3].id(), 3);
        assert_eq!(root.0.layers[2].id(), 4);
        assert_eq!(root.0.layers[1].id(), 5);
        assert_eq!(root.0.layers[1].as_group().unwrap().layers[0].id(), 6);
        assert_eq!(root.0.layers[0].id(), 7);

        assert_eq!(root.get_layer(6).unwrap().id(), 6);

        #[rustfmt::skip]
        let new_order = [
            3, 5,
                0, 4,
                0, 3,
                0, 1,
            0, 2,
            0, 7, // empty group
            0, 6,
        ];

        let reordered = root.reordered(0, &new_order).unwrap();

        assert_eq!(reordered.0.layers.len(), 4);
        assert_eq!(reordered.0.layers[0].id(), 5);
        assert_eq!(reordered.0.layers[1].id(), 2);
        assert_eq!(reordered.0.layers[2].id(), 7);
        assert_eq!(reordered.0.layers[3].id(), 6);

        let g = reordered.0.layers[0].as_group().unwrap();
        assert_eq!(g.layers.len(), 3);
        assert_eq!(g.layers[0].id(), 4);
        assert_eq!(g.layers[1].id(), 3);
        assert_eq!(g.layers[2].id(), 1);

        assert_eq!(
            // in root
            reordered.get_layer(2).unwrap().id(),
            2
        );
        assert_eq!(
            // in subgroup
            reordered.get_layer(1).unwrap().id(),
            1
        );
    }

    #[test]
    fn test_ungrouping() {
        let mut group = RootGroup::new(64, 64);

        assert!(group.add_group_layer(1, LayerInsertion::Top).is_some());

        assert!(group
            .add_bitmap_layer(2, Color::TRANSPARENT, LayerInsertion::Into(1))
            .is_some());

        assert!(group
            .add_bitmap_layer(3, Color::TRANSPARENT, LayerInsertion::Into(1))
            .is_some());

        #[rustfmt::skip]
        let new_order = [
            0, 1, // leave group empty
            0, 2,
            0, 3,
        ];
        let reordered = group.reordered(0, &new_order).unwrap();

        assert_eq!(reordered.0.layers.len(), 3);
        assert_eq!(reordered.0.layers[0].id(), 1);
        assert_eq!(reordered.0.layers[1].id(), 2);
        assert_eq!(reordered.0.layers[2].id(), 3);
        assert_eq!(reordered.0.layers[0].as_group().unwrap().layers.len(), 0);
    }

    #[test]
    fn test_layer_partial_reordering() {
        let mut group = RootGroup::new(64, 64);

        assert!(group.add_group_layer(1, LayerInsertion::Top).is_some());
        assert!(group.add_group_layer(2, LayerInsertion::Top).is_some());

        // should fail: not all layers included
        assert!(group.reordered(0, &[0, 1]).is_err());
    }

    #[test]
    fn test_subgroup_reordering() {
        let mut root = RootGroup::new(64, 64);
        root.add_group_layer(1, LayerInsertion::Top);
        root.add_group_layer(2, LayerInsertion::Top);

        root.add_bitmap_layer(10, Color::TRANSPARENT, LayerInsertion::Into(1));
        root.add_bitmap_layer(11, Color::TRANSPARENT, LayerInsertion::Into(1));

        root.add_bitmap_layer(20, Color::TRANSPARENT, LayerInsertion::Into(2));
        root.add_bitmap_layer(21, Color::TRANSPARENT, LayerInsertion::Into(2));
        root.add_bitmap_layer(22, Color::TRANSPARENT, LayerInsertion::Into(2));

        let reordered = root.reordered(2, &[0, 21, 0, 20, 0, 22]).unwrap();

        assert_eq!(reordered.0.layervec()[0].id(), 2);
        assert_eq!(reordered.0.layervec()[1].id(), 1);

        let g1 = reordered.get_layer(1).unwrap().as_group().unwrap();
        assert_eq!(g1.layervec()[0].id(), 11);
        assert_eq!(g1.layervec()[1].id(), 10);

        let g2 = reordered.get_layer(2).unwrap().as_group().unwrap();
        assert_eq!(g2.layervec()[0].id(), 21);
        assert_eq!(g2.layervec()[1].id(), 20);
        assert_eq!(g2.layervec()[2].id(), 22);
    }
}
