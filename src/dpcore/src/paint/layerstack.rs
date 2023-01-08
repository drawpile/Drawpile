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

use std::sync::Arc;

use super::annotation::{Annotation, AnnotationID, VAlign};
use super::aoe::AoE;
use super::color::{pixels15_to_8, ZERO_PIXEL15};
use super::flattenediter::FlattenedTileIterator;
use super::grouplayer::RootGroup;
use super::rasterop::tint_pixels;
use super::rectiter::{MutableRectIterator, RectIterator};
use super::tile::{Tile, TileData, TILE_SIZE, TILE_SIZEI};
use super::{
    BitmapLayer, Blendmode, Color, Frame, GroupLayer, Image8, LayerID, Pixel8, Rectangle, Timeline,
    UserID,
};

/// A layer stack wraps together the parts that make up the canvas
#[derive(Clone)]
pub struct LayerStack {
    root: Arc<RootGroup>,
    annotations: Arc<Vec<Arc<Annotation>>>,
    metadata: Arc<DocumentMetadata>,
    timeline: Arc<Timeline>,
    pub background: Tile,
}

/// Properties related to the whole document
#[derive(Clone, PartialEq)]
#[repr(C)]
pub struct DocumentMetadata {
    /// Horizontal resolution (not used by Drawpile)
    pub dpix: i32,

    /// Vertical resolution (not used by Drawpile)
    pub dpiy: i32,

    /// Framerate for animation export
    pub framerate: i32,

    /// Is the animation timeline enabled
    /// (if not, a simple automatic timeline is used.)
    pub use_timeline: bool,
}

impl Default for DocumentMetadata {
    fn default() -> Self {
        Self {
            dpix: 72,
            dpiy: 72,
            framerate: 24,
            use_timeline: false,
        }
    }
}

#[derive(PartialEq)]
#[repr(C)]
pub enum LayerViewMode {
    /// The normal rendering mode (all visible layers rendered)
    Normal,

    /// Render only the selected layer
    Solo,

    /// Render only the selected frame (root level layer) + fixed layers
    Frame,

    /// Render selected frame + few layers above and below with decreased
    /// opacity and optional color tint.
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

    /// The frame to render in Frame mode
    pub active_frame: Frame,

    /// Frames to render below the active one in onionskin mode
    pub frames_below: Vec<Frame>,

    /// Frames to render above the active one in onionskin mode
    pub frames_above: Vec<Frame>,

    /// Index of the active frame for Solo mode
    pub active_layer_id: LayerID,

    /// Don't render the canvas background
    pub no_canvas_background: bool,

    /// The background to use. This can be used to add (for example)
    /// a checkerboard texture to transparent areas.
    background: Tile,
}

impl Default for LayerViewOptions {
    fn default() -> Self {
        LayerViewOptions {
            censor: false,
            highlight: 0,
            viewmode: LayerViewMode::Normal,
            onionskin_tint: false,
            active_frame: Frame::empty(),
            frames_below: Vec::new(),
            frames_above: Vec::new(),
            active_layer_id: 0,
            no_canvas_background: false,
            background: Tile::Blank,
        }
    }
}

impl LayerViewOptions {
    /// Make an option set for showing a single frame
    ///
    /// Note: Since layers are ordered from top-to-bottom,
    /// the index numbers are reversed. The first frame is
    /// the bottom-most layer, thefore its index number
    /// is actually `layer_count() - 1`
    pub fn frame(frame: Frame) -> Self {
        Self {
            viewmode: LayerViewMode::Frame,
            active_frame: frame,
            ..Self::default()
        }
    }

    pub fn with_background(mut self, background: Tile) -> Self {
        self.background = background;

        self
    }
}

impl LayerStack {
    pub fn new(width: u32, height: u32) -> LayerStack {
        LayerStack {
            root: Arc::new(RootGroup::new(width, height)),
            annotations: Arc::new(Vec::<Arc<Annotation>>::new()),
            metadata: Arc::new(DocumentMetadata::default()),
            timeline: Arc::new(Timeline::new()),
            background: Tile::Blank,
        }
    }

    pub fn from_parts(
        root: Arc<RootGroup>,
        annotations: Arc<Vec<Arc<Annotation>>>,
        metadata: Arc<DocumentMetadata>,
        timeline: Arc<Timeline>,
        background: Tile,
    ) -> Self {
        Self {
            root,
            annotations,
            metadata,
            timeline,
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

    /// Get the whole document metadata
    pub fn metadata(&self) -> &DocumentMetadata {
        &self.metadata
    }

    pub fn metadata_mut(&mut self) -> &mut DocumentMetadata {
        Arc::make_mut(&mut self.metadata)
    }

    /// Get the animation timeline
    pub fn timeline(&self) -> &Arc<Timeline> {
        &self.timeline
    }

    pub fn timeline_mut(&mut self) -> &mut Timeline {
        Arc::make_mut(&mut self.timeline)
    }

    /// Get the frame at the given index
    ///
    /// If the timeline is not enabled, a simple automatic one
    /// is generated where each root-level layer correspond to a frame.
    pub fn frame_at(&self, index: isize) -> Frame {
        if index < 0 {
            return Frame::empty();
        }

        let index = index as usize;

        if self.metadata().use_timeline {
            *self.timeline.frames.get(index).unwrap_or(&Frame::empty())
        } else if index < self.root.layer_count() {
            // Note: first layer is at the bottom of the stack
            Frame::single(self.root.layer_at(self.root.layer_count() - index - 1).id())
        } else {
            Frame::empty()
        }
    }

    /// Get the number of animation frames in this layerstack
    pub fn frame_count(&self) -> usize {
        if self.metadata.use_timeline {
            self.timeline.frames.len()
        } else {
            self.root.layer_count()
        }
    }

    pub fn reordered(
        &self,
        root_group: LayerID,
        new_order: &[LayerID],
    ) -> Result<LayerStack, &'static str> {
        Ok(Self {
            root: Arc::new(self.root.reordered(root_group, new_order)?),
            annotations: self.annotations.clone(),
            metadata: self.metadata.clone(),
            timeline: self.timeline.clone(),
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
            rect,
            background: Color::TRANSPARENT,
            protect: false,
            valign: VAlign::Top,
            border: 0,
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
        self.annotations.iter().find_map(|a| {
            a.rect
                .expanded(expand)
                .contains_point(x, y)
                .then(|| a.as_ref())
        })
    }

    pub fn find_available_annotation_id(&self, for_user: UserID) -> AnnotationID {
        let prefix = (for_user as AnnotationID) << 8;

        #[allow(clippy::needless_collect)] // clippy is wrong, iterator would be used inside a loop
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

        0
    }

    /// Flatten layer stack content
    pub fn flatten_tile(&self, i: u32, j: u32, opts: &LayerViewOptions) -> TileData {
        let mut destination = if opts.no_canvas_background {
            Tile::Blank.clone_data()
        } else {
            self.background.clone_data()
        };

        if (i * TILE_SIZE) < self.root.width() && (j * TILE_SIZE) < self.root.height() {
            match opts.viewmode {
                LayerViewMode::Solo => {
                    if let Some(l) = self.root().get_layer(opts.active_layer_id) {
                        l.flatten_tile(&mut destination, i, j, 1.0, opts.censor, opts.highlight);
                    }
                }
                _ => {
                    Self::inner_flatten_tile(&mut destination, self.root.inner_ref(), i, j, opts);
                }
            }
        }

        if matches!(opts.background, Tile::Blank) {
            destination
        } else {
            let mut result = opts.background.clone_data();
            result.merge_data(&destination, 1.0, Blendmode::Normal);
            result
        }
    }

    fn inner_flatten_tile(
        destination: &mut TileData,
        root: &GroupLayer,
        i: u32,
        j: u32,
        opts: &LayerViewOptions,
    ) {
        for layer in root.iter_layers().rev() {
            let metadata = layer.metadata();
            // Don't render hidden layers
            if metadata.hidden {
                continue;
            }

            // Virtual parent opacity is set by special view modes
            // to hide or fade out layers (solo/onionskin)
            let (opacity, tint) = match opts.viewmode {
                LayerViewMode::Normal => (1.0, 0),
                LayerViewMode::Solo => unreachable!(),
                LayerViewMode::Frame | LayerViewMode::Onionskin => {
                    if !metadata.isolated && layer.is_group() {
                        // descend into non-isolated groups.
                        // this allows related layer frames to be kept side by side,
                        // rather than interspersed along the layerstack root
                        Self::inner_flatten_tile(
                            destination,
                            layer.as_group().unwrap(),
                            i,
                            j,
                            opts,
                        );
                        (0.0, 0)
                    } else if opts.active_frame.contains(metadata.id) {
                        (1.0, 0)
                    } else if matches!(opts.viewmode, LayerViewMode::Onionskin) {
                        if let Some(below) = Frame::find(metadata.id, &opts.frames_below) {
                            (
                                (1.0 - below as f32 / (opts.frames_below.len() + 1) as f32).powi(2),
                                if opts.onionskin_tint { 0x80_ff3333 } else { 0 },
                            )
                        } else if let Some(above) = Frame::find(metadata.id, &opts.frames_above) {
                            (
                                (1.0 - above as f32 / (opts.frames_above.len() + 1) as f32).powi(2),
                                if opts.onionskin_tint { 0x80_3333ff } else { 0 },
                            )
                        } else {
                            (0.0, 0)
                        }
                    } else {
                        (0.0, 0)
                    }
                }
            };

            // No need to render fully transparent layers
            if opacity < 1.0 / 256.0 {
                continue;
            }

            // Tinting only works right in Normal blend mode
            if tint != 0 && layer.metadata().blendmode == Blendmode::Normal {
                let mut tmp = TileData::new(ZERO_PIXEL15, 0);
                layer.flatten_tile(&mut tmp, i, j, opacity, opts.censor, opts.highlight);
                tint_pixels(&mut tmp.pixels, Color::from_argb32(tint));
                destination.merge_data(&tmp, 1.0, layer.metadata().blendmode);
            } else {
                layer.flatten_tile(destination, i, j, opacity, opts.censor, opts.highlight);
            }
        }
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

            Color::from_pixel15(tile.pixels[(ty * TILE_SIZE + tx) as usize])
        } else {
            let r = (dia / 2).min(1);

            let mut tmp = BitmapLayer::new(
                0,
                self.root.width(),
                self.root.height(),
                self.background.clone(),
            );

            let opts = LayerViewOptions::default();

            tmp.tile_rect_mut(&Rectangle::new(x - r, y - r, dia, dia))
                .for_each(|(i, j, t)| {
                    *t = Tile::Bitmap(Arc::new(self.flatten_tile(i as u32, j as u32, &opts)))
                });

            tmp.sample_color(x, y, dia)
        }
    }

    /// Render a flattened layerstack (or a subregion) to the given pixel array
    ///
    /// The rectangle must be fully within the layer bounds.
    /// The length of the pixel slice must be rect width*height.
    pub fn to_pixels8(
        &self,
        rect: Rectangle,
        opts: &LayerViewOptions,
        pixels: &mut [Pixel8],
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

            for (destrow, srcrow) in destiter.zip(srciter) {
                pixels15_to_8(destrow, srcrow);
            }
        }

        Ok(())
    }

    /// Convert whole layerstack to a flat image
    pub fn to_image(&self, opts: &LayerViewOptions) -> Image8 {
        let mut image = Image8::new(self.root.width() as usize, self.root.height() as usize);

        self.to_pixels8(
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
            metadata: self.metadata.clone(),
            timeline: self.timeline.clone(),
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

    /// Return true if the metadata differs between the two layer stacks
    pub fn compare_metadata(&self, other: &LayerStack) -> bool {
        // metadata changes so rarely that a shallow comparison is close enough here
        !Arc::ptr_eq(&self.metadata, &other.metadata)
    }

    pub fn compare_timeline(&self, other: &LayerStack) -> bool {
        // timeline changes so rarely that a shallow comparison is close enough here
        !Arc::ptr_eq(&self.timeline, &other.timeline)
    }
}

#[cfg(test)]
mod tests {
    use super::super::grouplayer::*;
    use super::super::BIT15_U16;
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
        assert_eq!(
            t1.pixels[0],
            [BIT15_U16 / 2, BIT15_U16 / 2, BIT15_U16, BIT15_U16]
        );

        let t2 = stack.flatten_tile(1, 0, &opts);
        assert_eq!(t2.pixels[0], Color::rgb8(255, 255, 255).as_pixel15());
    }
}
