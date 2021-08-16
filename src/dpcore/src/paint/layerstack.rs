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

use std::rc::Rc;

use super::annotation::{Annotation, AnnotationID, VAlign};
use super::aoe::AoE;
use super::tile::{Tile, TileData, TILE_SIZE};
use super::{Color, Image, Layer, LayerID, Rectangle, Size};

#[derive(Clone)]
pub struct LayerStack {
    layers: Rc<Vec<Rc<Layer>>>,
    annotations: Rc<Vec<Rc<Annotation>>>,
    pub background: Tile,
    width: u32,
    height: u32,
}

#[derive(Clone)]
pub enum LayerFill {
    Solid(Color),
    Copy(LayerID),
}

pub enum LayerInsertion {
    Top,
    Above(LayerID),
    Bottom,
}

impl LayerStack {
    pub fn new(width: u32, height: u32) -> LayerStack {
        LayerStack {
            layers: Rc::new(Vec::<Rc<Layer>>::new()),
            annotations: Rc::new(Vec::<Rc<Annotation>>::new()),
            background: Tile::Blank,
            width,
            height,
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

    /// Add a new layer and return a mutable reference to it
    ///
    /// If a layer with the given ID exists already, None will be returned.
    /// If fill is Copy and the source layer does not exist, None will be returned.
    pub fn add_layer(
        &mut self,
        id: LayerID,
        fill: LayerFill,
        pos: LayerInsertion,
    ) -> Option<&mut Layer> {
        if self.find_layer_index(id).is_some() {
            return None;
        }

        let insert_idx = match pos {
            LayerInsertion::Top => self.layers.len(),
            LayerInsertion::Above(layer_id) => self.find_layer_index(layer_id)? + 1,
            LayerInsertion::Bottom => 0,
        };

        let new_layer = match fill {
            LayerFill::Solid(c) => Rc::new(Layer::new(id, self.width, self.height, &c)),
            LayerFill::Copy(src_id) => {
                let mut l = self.layers[self.find_layer_index(src_id)?].clone();
                Rc::make_mut(&mut l).id = id;
                l
            }
        };

        let layers = Rc::make_mut(&mut self.layers);
        layers.insert(insert_idx, new_layer);
        Some(Rc::make_mut(layers.last_mut().unwrap()))
    }

    /// Find a layer with the given ID and return a reference to it
    pub fn get_layer(&self, id: LayerID) -> Option<&Layer> {
        for l in self.layers.iter() {
            if l.id == id {
                return Some(l);
            }
        }
        None
    }

    /// Find a layer with the given ID and return a reference counted pointer to it
    pub fn get_layer_rc(&self, id: LayerID) -> Option<Rc<Layer>> {
        for l in self.layers.iter() {
            if l.id == id {
                return Some(l.clone());
            }
        }
        None
    }

    /// Find a layer with the given ID
    pub fn get_layer_mut(&mut self, id: LayerID) -> Option<&mut Layer> {
        if let Some(idx) = self.find_layer_index(id) {
            Some(Rc::make_mut(&mut Rc::make_mut(&mut self.layers)[idx]))
        } else {
            None
        }
    }

    /// Remove a layer with the given ID
    pub fn remove_layer(&mut self, id: LayerID) {
        if let Some(idx) = self.find_layer_index(id) {
            Rc::make_mut(&mut self.layers).remove(idx);
        }
    }

    /// Find the ID of the layer below this layer
    pub fn find_layer_below(&self, id: LayerID) -> Option<LayerID> {
        if let Some(idx) = self.find_layer_index(id) {
            if idx > 0 {
                return Some(self.layers[idx - 1].id);
            }
        }
        None
    }

    fn find_layer_index(&self, id: LayerID) -> Option<usize> {
        self.layers.iter().position(|l| l.id == id)
    }

    /// Return a copy of the layerstack with the layers in the given order.
    /// The new order vector is sanitized. Duplicate and nonexistent layers
    /// are dropped and missing layers are appended.
    pub fn reordered(&self, new_order: &[LayerID]) -> LayerStack {
        let mut ordered = Vec::<Rc<Layer>>::new();
        let mut oldorder = (*self.layers).clone();

        // Take layers from the old list and add them in the specified order.
        for &layer_id in new_order {
            if let Some(pos) = oldorder.iter().position(|l| l.id == layer_id) {
                ordered.push(oldorder.remove(pos));
            }
        }

        // Add any remaining layers in the existing order
        ordered.extend_from_slice(&oldorder);

        LayerStack {
            layers: Rc::new(ordered),
            annotations: self.annotations.clone(),
            background: self.background.clone(),
            ..*self
        }
    }

    /// Create a new (blank) annotation
    pub fn add_annotation(&mut self, id: AnnotationID, rect: Rectangle) {
        if self.find_annotation_index(id).is_some() {
            return;
        }
        Rc::make_mut(&mut self.annotations).push(Rc::new(Annotation {
            id,
            text: String::new(),
            rect: rect,
            background: Color::TRANSPARENT,
            protect: false,
            valign: VAlign::Top,
        }));
    }

    pub fn remove_annotation(&mut self, id: AnnotationID) {
        Rc::make_mut(&mut self.annotations).retain(|a| a.id != id);
    }

    pub fn get_annotation(&self, id: AnnotationID) -> Option<&Annotation> {
        if let Some(idx) = self.find_annotation_index(id) {
            Some(&self.annotations[idx])
        } else {
            None
        }
    }

    pub fn get_annotation_mut(&mut self, id: AnnotationID) -> Option<&mut Annotation> {
        if let Some(idx) = self.find_annotation_index(id) {
            Some(Rc::make_mut(&mut Rc::make_mut(&mut self.annotations)[idx]))
        } else {
            None
        }
    }

    fn find_annotation_index(&self, id: AnnotationID) -> Option<usize> {
        self.annotations.iter().position(|a| a.id == id)
    }

    /// Flatten layer stack content
    pub fn flatten_tile(&self, i: u32, j: u32) -> TileData {
        let mut destination = self.background.clone_data();

        if (i * TILE_SIZE) < self.width && (j * TILE_SIZE) < self.height {
            for layer in self.layers.iter() {
                layer.flatten_tile(&mut destination, i, j);
            }
        }

        destination
    }

    // Convert to a flat image
    pub fn to_image(&self) -> Image {
        let xtiles = Tile::div_up(self.width) as usize;
        let ytiles = Tile::div_up(self.height) as usize;

        let tw = TILE_SIZE as usize;
        let width = self.width as usize;
        let height = self.height as usize;

        let mut image = Image::new(width, height);

        for j in 0..ytiles {
            let h = tw.min(height - (j * tw));
            for i in 0..xtiles {
                let td = self.flatten_tile(i as u32, j as u32);
                let w = tw.min(width - (i * tw));
                for y in 0..h {
                    let dest_offset = (j * tw + y) * width + i * tw;
                    let src_offset = y * tw;

                    image.pixels[dest_offset..dest_offset + w]
                        .copy_from_slice(&td.pixels[src_offset..src_offset + w]);
                }
            }
        }

        image
    }

    /// Return a resized copy of this stack
    pub fn resized(&self, top: i32, right: i32, bottom: i32, left: i32) -> Option<LayerStack> {
        let new_width = left + self.width as i32 + right;
        let new_height = top + self.height as i32 + bottom;
        if new_width <= 0 || new_height <= 0 {
            return None;
        }

        Some(LayerStack {
            layers: Rc::new(
                self.layers
                    .iter()
                    .map(|l| Rc::new(l.resized(top, right, bottom, left)))
                    .collect(),
            ),
            annotations: Rc::new(
                self.annotations
                    .iter()
                    .map(|a| {
                        Rc::new(Annotation {
                            rect: a.rect.offset(left, top),
                            text: a.text.clone(),
                            ..**a
                        })
                    })
                    .collect(),
            ),
            background: self.background.clone(),
            width: new_width as u32,
            height: new_height as u32,
        })
    }

    pub fn iter_layers(&self) -> impl Iterator<Item = &Layer> {
        return self.layers.iter().map(|l| l.as_ref());
    }

    pub fn iter_layers_mut(&mut self) -> impl Iterator<Item = &mut Rc<Layer>> {
        return Rc::make_mut(&mut self.layers).iter_mut();
    }

    /// Compare this layer stack with the other and return an Area Of Effect.
    /// This is used when the entire layer stack is replaced, e.g. when
    /// resetting to an earlier state as a part of an undo operation.
    /// For performance reasons, shallow comparison is used.
    pub fn compare(&self, other: &LayerStack) -> AoE {
        if self.width != other.width || self.height != other.height {
            return AoE::Resize(0, 0, self.size());
        }
        if Rc::ptr_eq(&self.layers, &other.layers) {
            return AoE::Nothing;
        }
        if self.layers.len() != other.layers.len() {
            return AoE::Everything;
        }

        self.layers
            .iter()
            .zip(other.layers.iter())
            .fold(AoE::Nothing, |aoe, (a, b)| aoe.merge(a.compare(b)))
    }

    /// Compare this layer stack with the other and return true if they
    /// differ structurally (different number of layers, different order,
    /// or different layer attributes.)
    pub fn compare_layer_structure(&self, other: &LayerStack) -> bool {
        if self.layers.len() != other.layers.len() {
            return true;
        }

        for (a, b) in self.layers.iter().zip(other.layers.iter()) {
            if !Rc::ptr_eq(a, b) {
                if a.id != b.id
                    || a.title != b.title
                    || a.opacity != b.opacity
                    || a.hidden != b.hidden
                    || a.censored != b.censored
                    || a.fixed != b.fixed
                    || a.blendmode != b.blendmode
                {
                    return true;
                }
            }
        }

        false
    }

    /// Return true if the annotations differ between the two layer stacks
    pub fn compare_annotations(&self, other: &LayerStack) -> bool {
        !Rc::ptr_eq(&self.annotations, &other.annotations)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_layer_addition() {
        let mut stack = LayerStack::new(256, 256);
        assert!(stack
            .add_layer(1, LayerFill::Solid(Color::TRANSPARENT), LayerInsertion::Top)
            .is_some());

        // Adding a layer with an existing ID does nothing
        assert!(stack
            .add_layer(1, LayerFill::Solid(Color::TRANSPARENT), LayerInsertion::Top)
            .is_none());

        // One more layer on top
        assert!(stack
            .add_layer(
                2,
                LayerFill::Solid(Color::rgb8(255, 0, 0)),
                LayerInsertion::Top
            )
            .is_some());

        // Duplicate layer on top
        assert!(stack
            .add_layer(3, LayerFill::Copy(1), LayerInsertion::Top)
            .is_some());

        // Insert layer above the bottom-most
        assert!(stack
            .add_layer(
                4,
                LayerFill::Solid(Color::rgb8(0, 255, 0)),
                LayerInsertion::Above(1)
            )
            .is_some());

        // Insert layer at the bottom
        assert!(stack
            .add_layer(
                5,
                LayerFill::Solid(Color::rgb8(0, 0, 255)),
                LayerInsertion::Bottom
            )
            .is_some());

        // Insert layer above the topmost
        assert!(stack
            .add_layer(
                6,
                LayerFill::Solid(Color::rgb8(0, 0, 255)),
                LayerInsertion::Above(3)
            )
            .is_some());

        assert!(stack.get_layer(0).is_none());
        assert_eq!(stack.layers[0].id, 5);
        assert_eq!(stack.layers[1].id, 1);
        assert_eq!(stack.layers[2].id, 4);
        assert_eq!(stack.layers[3].id, 2);
        assert_eq!(stack.layers[4].id, 3);
        assert_eq!(stack.layers[5].id, 6);
    }

    #[test]
    fn test_layer_removal() {
        let mut stack = LayerStack::new(256, 256);
        stack.add_layer(1, LayerFill::Solid(Color::TRANSPARENT), LayerInsertion::Top);
        stack.add_layer(2, LayerFill::Solid(Color::TRANSPARENT), LayerInsertion::Top);
        stack.add_layer(3, LayerFill::Solid(Color::TRANSPARENT), LayerInsertion::Top);

        assert_eq!(stack.layers.len(), 3);
        stack.remove_layer(2);
        assert_eq!(stack.layers.len(), 2);
        assert!(stack.get_layer(1).is_some());
        assert!(stack.get_layer(2).is_none());
        assert!(stack.get_layer(3).is_some());
    }

    #[test]
    fn test_layer_reordering() {
        let mut stack = LayerStack::new(64, 64);
        assert!(stack
            .add_layer(1, LayerFill::Solid(Color::TRANSPARENT), LayerInsertion::Top)
            .is_some());
        assert!(stack
            .add_layer(2, LayerFill::Solid(Color::TRANSPARENT), LayerInsertion::Top)
            .is_some());
        assert!(stack
            .add_layer(3, LayerFill::Solid(Color::TRANSPARENT), LayerInsertion::Top)
            .is_some());
        assert!(stack
            .add_layer(4, LayerFill::Solid(Color::TRANSPARENT), LayerInsertion::Top)
            .is_some());

        // duplicates are silently dropped and missing layers appended in the original order
        let new_order = [3, 2, 2, 3];
        let reordered = stack.reordered(&new_order);

        assert_eq!(reordered.layers[0].id, 3);
        assert_eq!(reordered.layers[1].id, 2);
        assert_eq!(reordered.layers[2].id, 1);
        assert_eq!(reordered.layers[3].id, 4);
    }

    #[test]
    fn test_flattening() {
        let mut stack = LayerStack::new(128, 64);
        stack.background = Tile::new_solid(&Color::rgb8(255, 255, 255), 0);
        stack.add_layer(1, LayerFill::Solid(Color::TRANSPARENT), LayerInsertion::Top);
        stack.add_layer(2, LayerFill::Solid(Color::TRANSPARENT), LayerInsertion::Top);

        let layer = stack.get_layer_mut(1).unwrap();
        *layer.tile_mut(0, 0) = Tile::new_solid(&Color::rgb8(255, 0, 0), 0);
        layer.opacity = 0.5;

        let t1 = stack.flatten_tile(0, 0);
        assert_eq!(t1.pixels[0], Color::rgb8(255, 128, 128).as_pixel());

        let t2 = stack.flatten_tile(1, 0);
        assert_eq!(t2.pixels[0], Color::rgb8(255, 255, 255).as_pixel());
    }
}
