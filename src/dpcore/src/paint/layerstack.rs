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

use std::cell::RefCell;
use std::sync::Arc;

use super::annotation::{Annotation, AnnotationID, VAlign};
use super::aoe::AoE;
use super::color::ZERO_PIXEL;
use super::flattenediter::FlattenedTileIterator;
use super::grouplayer::RootGroup;
use super::rasterop::tint_pixels;
use super::rectiter::{MutableRectIterator, RectIterator};
use super::tile::{Tile, TileData, TILE_SIZE, TILE_SIZEI};
use super::{
    BitmapLayer, Blendmode, Color, Image, InternalLayerID, LayerID, Pixel, Rectangle,
    UserID,
};

/// A layer stack wraps together the parts that make up the canvas
#[derive(Clone)]
pub struct LayerStack {
    root: Arc<RootGroup>,
    annotations: Arc<Vec<Arc<Annotation>>>,
    pub background: Tile,
}

#[derive(PartialEq)]
#[repr(C)]
pub enum LayerViewMode {
    Normal,
    Solo,
    Onionskin,
}

/// Layer flattening options
pub struct LayerViewOptions {
    /// Replace tiles of censored layers with a censor pattern
    pub censor: bool,

    /// Paint a highlight pattern over all non-blank tiles last touched by this user
    pub highlight: UserID,

    /// Special layer view modes
    pub viewmode: LayerViewMode,

    /// Add a color tint to onionskin layers
    pub onionskin_tint: bool,

    /// Number of onionskin layers to show above the active one
    pub onionskins_above: i32,

    /// Number of onionskin layers to show below the active one
    pub onionskins_below: i32,

    /// Index of the active layer for solo and onionskin mode
    pub active_layer_idx: usize,

    /// The background to use. This can be used to add (for example)
    /// a checkerboard texture to transparent areas.
    background: Tile,

    /// Cached version of the composited background (layerstack bg, composited bg)
    background_cache: RefCell<(Tile, Tile)>,
}

impl Default for LayerViewOptions {
    fn default() -> Self {
        LayerViewOptions {
            censor: false,
            highlight: 0,
            viewmode: LayerViewMode::Normal,
            onionskin_tint: false,
            onionskins_above: 1,
            onionskins_below: 1,
            active_layer_idx: 0,
            background: Tile::Blank,
            background_cache: RefCell::new((Tile::Blank, Tile::Blank)),
        }
    }
}

impl LayerViewOptions {
    pub fn solo(index: usize) -> Self {
        Self {
            censor: false,
            highlight: 0,
            viewmode: LayerViewMode::Solo,
            onionskin_tint: false,
            onionskins_above: 1,
            onionskins_below: 1,
            active_layer_idx: index,
            background: Tile::Blank,
            background_cache: RefCell::new((Tile::Blank, Tile::Blank)),
        }
    }

    pub fn with_background(mut self, background: Tile) -> Self {
        self.background = background.clone();
        self.background_cache.replace((Tile::Blank, background));

        self
    }
}

impl LayerStack {
    pub fn new(width: u32, height: u32) -> LayerStack {
        LayerStack {
            root: Arc::new(RootGroup::new(width, height)),
            annotations: Arc::new(Vec::<Arc<Annotation>>::new()),
            background: Tile::Blank,
        }
    }

    pub fn from_parts(
        root: Arc<RootGroup>,
        annotations: Arc<Vec<Arc<Annotation>>>,
        background: Tile,
    ) -> LayerStack {
        LayerStack {
            root: root,
            annotations,
            background,
        }
    }

    /// Get the root of the layer stack
    pub fn root(&self) -> &RootGroup {
        &self.root
    }

    pub fn root_mut(&mut self) -> &mut RootGroup {
        Arc::make_mut(&mut self.root)
    }

    /// Find the layer index corresponding to this frame index
    /// Fixed and hidden layers are not considered frames.
    pub fn find_frame_index(&self, frame: usize) -> Option<usize> {
        let mut frames = frame + 1;
        for (idx, layer) in self.root.iter_layers().enumerate() {
            if !layer.metadata().fixed && layer.is_visible() {
                frames -= 1;
                if frames == 0 {
                    return Some(idx);
                }
            }
        }
        None
    }

    /// Get the number of animation frames in the layerstack
    ///
    /// A frame is represented by a toplevel layer that is not fixed or hidden.
    pub fn frame_count(&self) -> usize {
        self.root
            .iter_layers()
            .filter(|l| !l.metadata().fixed && l.is_visible())
            .count()
    }

    pub fn reordered(&self, new_order: &[LayerID]) -> Result<LayerStack, &'static str> {
        Ok(Self {
            root: Arc::new(self.root.reordered(new_order)?),
            annotations: self.annotations.clone(),
            background: self.background.clone(),
        })
    }

    /// Create a new (blank) annotation
    pub fn add_annotation(&mut self, id: AnnotationID, rect: Rectangle) {
        if self.find_annotation_index(id).is_some() {
            return;
        }
        Arc::make_mut(&mut self.annotations).push(Arc::new(Annotation {
            id,
            text: String::new(),
            rect: rect,
            background: Color::TRANSPARENT,
            protect: false,
            valign: VAlign::Top,
        }));
    }

    pub fn remove_annotation(&mut self, id: AnnotationID) {
        Arc::make_mut(&mut self.annotations).retain(|a| a.id != id);
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
            Some(Arc::make_mut(
                &mut Arc::make_mut(&mut self.annotations)[idx],
            ))
        } else {
            None
        }
    }

    fn find_annotation_index(&self, id: AnnotationID) -> Option<usize> {
        self.annotations.iter().position(|a| a.id == id)
    }

    pub fn get_annotations(&self) -> Arc<Vec<Arc<Annotation>>> {
        self.annotations.clone()
    }

    /// Find the annotation at the given coordinates.
    ///
    /// If expand is nonzero, the annotations' bounds will be expanded in all
    /// directions by the given amount. This is used to account for the
    /// grab handles drawn in the UI.
    pub fn get_annotation_at(&self, x: i32, y: i32, expand: i32) -> Option<&Annotation> {
        for a in self.annotations.iter() {
            if a.rect.expanded(expand).contains_point(x, y) {
                return Some(a);
            }
        }
        None
    }

    pub fn find_available_annotation_id(&self, for_user: UserID) -> AnnotationID {
        let prefix = (for_user as AnnotationID) << 8;

        let taken_ids: Vec<AnnotationID> = self
            .annotations
            .iter()
            .map(|a| a.id)
            .filter(|id| id & 0xff00 == prefix)
            .collect();

        for id in 0..256 {
            if !taken_ids.contains(&(prefix | id)) {
                return prefix | id;
            }
        }

        return 0;
    }

    /// Flatten layer stack content
    pub fn flatten_tile(&self, i: u32, j: u32, opts: &LayerViewOptions) -> TileData {
        // Cache the composited background tile
        let mut destination = {
            if matches!(opts.background, Tile::Blank) {
                self.background.clone_data()
            } else {
                let cache = opts.background_cache.borrow();
                if cache.0.ptr_eq(&self.background) {
                    cache.1.clone_data()
                } else {
                    drop(cache);
                    let mut composited = opts.background.clone();
                    composited.merge(&self.background, 1.0, Blendmode::Normal);
                    opts.background_cache
                        .replace((self.background.clone(), composited.clone()));
                    composited.clone_data()
                }
            }
        };

        // The root group is special.
        // Onionskin, solo and other animation features only apply to
        // the root, as only root level layers are treated as frames.
        if (i * TILE_SIZE) < self.root.width() && (j * TILE_SIZE) < self.root.height() {
            for (idx, layer) in self.root.iter_layers().rev().enumerate() {
                let metadata = layer.metadata();
                // Don't render hidden layers
                if metadata.hidden {
                    continue;
                }

                // Virtual parent opacity is set by special view modes
                // to hide or fade out layers (solo/onionskin)
                let (opacity, tint) = match opts.viewmode {
                    LayerViewMode::Normal => (1.0, 0),
                    LayerViewMode::Solo => (
                        if idx == opts.active_layer_idx || metadata.fixed {
                            1.0
                        } else {
                            0.0
                        },
                        0,
                    ),
                    LayerViewMode::Onionskin => {
                        if metadata.fixed {
                            (1.0, 0)
                        } else {
                            let d = opts.active_layer_idx as i32 - idx as i32;
                            let rd = if d == 0 {
                                0.0
                            } else if d < 0 && d >= -opts.onionskins_above {
                                -d as f32 / (opts.onionskins_above + 1) as f32
                            } else if d > 0 && d <= opts.onionskins_below {
                                d as f32 / (opts.onionskins_below + 1) as f32
                            } else {
                                1.0
                            };
                            (
                                (1.0 - rd).powi(2),
                                if d == 0 || !opts.onionskin_tint {
                                    0
                                } else if d < 0 {
                                    0x80_3333ff
                                } else {
                                    0x80_ff3333
                                },
                            )
                        }
                    }
                };

                // No need to render fully transparent layers
                if opacity < 1.0 / 256.0 {
                    continue;
                }

                // Tinting only works right in Normal blend mode
                if tint != 0 && layer.metadata().blendmode == Blendmode::Normal {
                    let mut tmp = TileData::new(ZERO_PIXEL, 0);
                    layer.flatten_tile(&mut tmp, i, j, opacity, opts.censor, opts.highlight);
                    tint_pixels(&mut tmp.pixels, Color::from_argb32(tint));
                    destination.merge_data(&mut tmp, 1.0, layer.metadata().blendmode);
                } else {
                    layer.flatten_tile(
                        &mut destination,
                        i,
                        j,
                        opacity,
                        opts.censor,
                        opts.highlight,
                    );
                }
            }
        }

        destination
    }

    /// Get a weighted average of a color
    pub fn sample_color(&self, x: i32, y: i32, dia: i32) -> Color {
        if x < 0 || y < 0 || x as u32 >= self.root.width() || y as u32 >= self.root.height() {
            return Color::TRANSPARENT;
        }

        // TODO some more efficient way of doing this
        if dia <= 1 {
            let ti = x as u32 / TILE_SIZE;
            let tj = y as u32 / TILE_SIZE;
            let tile = self.flatten_tile(ti, tj, &LayerViewOptions::default());

            let tx = x as u32 - ti * TILE_SIZE;
            let ty = y as u32 - tj * TILE_SIZE;

            Color::from_pixel(tile.pixels[(ty * TILE_SIZE + tx) as usize])
        } else {
            let r = (dia / 2).min(1) as i32;

            let mut tmp = BitmapLayer::new(
                InternalLayerID(0),
                self.root.width(),
                self.root.height(),
                self.background.clone(),
            );

            let opts = LayerViewOptions::default();

            tmp.tile_rect_mut(&Rectangle::new(
                x as i32 - r,
                y as i32 - r,
                dia as i32,
                dia as i32,
            ))
            .for_each(|(i, j, t)| {
                *t = Tile::Bitmap(Arc::new(self.flatten_tile(i as u32, j as u32, &opts)))
            });

            tmp.sample_color(x as i32, y as i32, dia)
        }
    }

    /// Render a flattened layerstack (or a subregion) to the given pixel array
    ///
    /// The rectangle must be fully within the layer bounds.
    /// The length of the pixel slice must be rect width*height.
    pub fn to_pixels(
        &self,
        rect: Rectangle,
        opts: &LayerViewOptions,
        pixels: &mut [Pixel],
    ) -> Result<(), &str> {
        if !rect.in_bounds(self.root.size()) {
            return Err("source rectangle out of bounds");
        }
        if pixels.len() != (rect.w * rect.h) as usize {
            return Err("pixel slice length does not match source rectangle size");
        }

        for (i, j, tile) in FlattenedTileIterator::new(self, opts, rect.into()) {
            // This tile's bounding rectangle
            let tilerect = Rectangle::tile(i, j, TILE_SIZEI);

            // The rectangle of interest in the canvas
            let canvasrect = rect.intersected(&tilerect).unwrap();

            // The source rectangle in the tile
            let srcrect = canvasrect.offset(-tilerect.x, -tilerect.y);

            // The destination rectangle in the pixel buffer
            let pixelrect = canvasrect.offset(-rect.x, -rect.y);

            // Copy pixels from tile to image
            let srciter = RectIterator::from_rectangle(&tile.pixels, TILE_SIZE as usize, &srcrect);
            let destiter = MutableRectIterator::new(
                pixels,
                rect.w as usize,
                pixelrect.x as usize,
                pixelrect.y as usize,
                pixelrect.w as usize,
                pixelrect.h as usize,
            );

            destiter
                .zip(srciter)
                .for_each(|(d, s)| d.copy_from_slice(s));
        }

        Ok(())
    }

    /// Convert whole layerstack to a flat image
    pub fn to_image(&self, opts: &LayerViewOptions) -> Image {
        let mut image = Image::new(self.root.width() as usize, self.root.height() as usize);

        self.to_pixels(
            Rectangle::new(0, 0, self.root.width() as i32, self.root.height() as i32),
            opts,
            &mut image.pixels,
        )
        .unwrap();

        image
    }

    /// Return a resized copy of this stack
    pub fn resized(&self, top: i32, right: i32, bottom: i32, left: i32) -> Option<LayerStack> {
        Some(LayerStack {
            root: Arc::new(self.root.resized(top, right, bottom, left)?),
            annotations: Arc::new(
                self.annotations
                    .iter()
                    .map(|a| {
                        Arc::new(Annotation {
                            rect: a.rect.offset(left, top),
                            text: a.text.clone(),
                            ..**a
                        })
                    })
                    .collect(),
            ),
            background: self.background.clone(),
        })
    }

    /// Compare this layer stack with the other and return an Area Of Effect.
    /// This is used when the entire layer stack is replaced, e.g. when
    /// resetting to an earlier state as a part of an undo operation.
    /// For performance reasons, shallow comparison is used.
    pub fn compare(&self, other: &LayerStack) -> AoE {
        if Arc::ptr_eq(&self.root, &other.root) {
            return AoE::Nothing;
        }

        self.root.compare(&other.root)
    }

    /// Return true if the annotations differ between the two layer stacks
    pub fn compare_annotations(&self, other: &LayerStack) -> bool {
        !Arc::ptr_eq(&self.annotations, &other.annotations)
    }
}

#[cfg(test)]
mod tests {
    use super::super::grouplayer::*;
    use super::*;

    #[test]
    fn test_flattening() {
        let mut stack = LayerStack::new(128, 64);
        let opts = LayerViewOptions::default();
        stack.background = Tile::new_solid(&Color::rgb8(255, 255, 255), 0);
        stack
            .root_mut()
            .add_bitmap_layer(1, Color::TRANSPARENT, LayerInsertion::Top);
        stack
            .root_mut()
            .add_bitmap_layer(2, Color::TRANSPARENT, LayerInsertion::Top);

        let layer = stack.root_mut().get_bitmaplayer_mut(1).unwrap();
        *layer.tile_mut(0, 0) = Tile::new_solid(&Color::rgb8(255, 0, 0), 0);
        layer.metadata_mut().opacity = 0.5;

        let t1 = stack.flatten_tile(0, 0, &opts);
        assert_eq!(t1.pixels[0], Color::rgb8(255, 128, 128).as_pixel());

        let t2 = stack.flatten_tile(1, 0, &opts);
        assert_eq!(t2.pixels[0], Color::rgb8(255, 255, 255).as_pixel());
    }
}
