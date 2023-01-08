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

use dpcore::canvas::compression::compress_tile;
use dpcore::canvas::CanvasState;
use dpcore::paint::annotation::Annotation;
use dpcore::paint::{
    BitmapLayer, DocumentMetadata, GroupLayer, Layer, LayerMetadata, LayerViewOptions, Tile,
    Timeline,
};
use dpcore::protocol::message::CommandMessage;

use super::{IndexEntry, IndexResult, INDEX_FORMAT_VERSION};
use crate::conv::from_dpimage;

use byteorder::{LittleEndian, WriteBytesExt};
use image::codecs::png::PngEncoder;
use image::{imageops, EncodableLayout, ImageEncoder};
use std::collections::HashMap;
use std::convert::TryInto;
use std::io::{Seek, SeekFrom, Write};
use std::sync::Arc;

// Caches for sharing subblocks.
// The key is the raw pointer of the data structure.
// The value is the offset of the subblock in the index file.
// A copy of the reference-counted data structure is kept in
// the value to keep the reference alive. Without it, the object
// may have been dropped the next time make_entry is called
// and the key pointer no longer valid.
type TileMap = HashMap<usize, (Tile, u64)>;
type LayerMap = HashMap<usize, (Arc<Layer>, u64)>;
type AnnotationMap = HashMap<usize, (Arc<Annotation>, u64)>;

pub struct IndexBuilder<W: Write + Seek> {
    /// The writer to write the index with
    writer: W,

    /// Number of messages read
    messages: u32,

    /// Offset of the last read message in the recording file
    last_message_offset: u64,

    /// State of the canvas (rendering engine)
    canvas: CanvasState,

    /// Index entries generated so far.
    /// These will be written at the end.
    entries: Vec<IndexEntry>,

    /// Subblocks used in the last entry.
    ///
    /// Typically, two adjacent entries will share most of their blocks.
    /// A block that wasn't referenced in the previous entry will not be
    /// referenced in the next one either, except potentially when undo is used,
    /// so we can achieve very good deduplication by just remembering
    /// what was included last.
    ///
    /// The key is the raw pointer of the original resource, which can
    /// be a tile, a layer or an annotation.
    last_tiles: TileMap,
    last_layers: LayerMap,
    last_annotations: AnnotationMap,
    last_metadata: (u64, DocumentMetadata),
    last_timeline: (usize, u64, Arc<Timeline>),
}

/// Statistics. Mainly for tuning and debugging purposes.
#[derive(Default)]
pub struct Stats {
    pub index: u32,
    pub new_tiles: u32,
    pub reused_tiles: u32,
    pub null_tiles: u32,
    pub changed_layers: u32,
    pub reused_layers: u32,
    pub changed_annotations: u32,
    pub reused_annotations: u32,
    pub metadata_changed: bool,
    pub timeline_changed: bool,
}

impl<W: Write + Seek> IndexBuilder<W> {
    pub fn new(writer: W) -> Self {
        Self {
            writer,
            messages: 0,
            last_message_offset: 0,
            canvas: CanvasState::new(),
            entries: Vec::new(),
            last_tiles: TileMap::new(),
            last_layers: LayerMap::new(),
            last_annotations: AnnotationMap::new(),
            last_metadata: (0, DocumentMetadata::default()),
            last_timeline: (0, 0, Arc::new(Timeline::new())),
        }
    }

    /// Write the index header.
    /// This should be called once before any other write function.
    pub fn write_header(&mut self) -> IndexResult<()> {
        self.writer.write_all(b"DPIDX\0")?;

        // Index format version number
        self.writer
            .write_u16::<LittleEndian>(INDEX_FORMAT_VERSION)?;

        // Placeholders for message count and index entry vector offset
        self.writer.write_u32::<LittleEndian>(0)?;
        self.writer.write_u64::<LittleEndian>(0)?;
        Ok(())
    }

    /// Add a new message to the builder state.
    /// Call make_entry to create a new index entry for current state.
    pub fn add_message(&mut self, m: &CommandMessage, offset: u64) {
        debug_assert!(offset > self.last_message_offset);

        self.canvas.receive_message(m);
        self.last_message_offset = offset;
        self.messages += 1;
    }

    /// Create a new index entry.
    /// A state snapshot is immediately written to the index file.
    pub fn make_entry(&mut self, with_thumbnail: bool) -> IndexResult<Stats> {
        assert!(self.messages > 0);

        let mut used_tiles = TileMap::new();
        let mut used_layers = LayerMap::new();
        let mut used_annotations = AnnotationMap::new();

        let mut stats = Stats {
            index: self.messages - 1,
            ..Stats::default()
        };

        let snapshot_offset = self.write_layerstack(
            &mut used_tiles,
            &mut used_layers,
            &mut used_annotations,
            &mut stats,
        )?;

        let thumbnail_offset = if with_thumbnail {
            self.write_thumbnail()?
        } else {
            0
        };

        self.entries.push(IndexEntry {
            message_index: self.messages - 1,
            message_offset: self.last_message_offset,
            snapshot_offset,
            thumbnail_offset,
        });

        // Remember only the subblocks used in the last entry.
        // This is enough, since once gone, they don't come back,
        // undos not withstanding.
        self.last_layers = used_layers;
        self.last_annotations = used_annotations;
        self.last_tiles = used_tiles;

        Ok(stats)
    }

    /// Finalize the index file by writing out the entry vector
    ///
    /// This should be called once after the whole recording has
    /// been processed.
    pub fn finalize(&mut self) -> IndexResult<()> {
        let offset = self.writer.stream_position()?;

        // Write the list of index entries
        for entry in self.entries.iter() {
            self.writer.write_u32::<LittleEndian>(entry.message_index)?;
            self.writer
                .write_u64::<LittleEndian>(entry.message_offset)?;
            self.writer
                .write_u64::<LittleEndian>(entry.snapshot_offset)?;
            self.writer
                .write_u64::<LittleEndian>(entry.thumbnail_offset)?;
        }

        // Jump back to the beginning to fill in the placeholders
        self.writer.seek(SeekFrom::Start(8))?;

        self.writer.write_u32::<LittleEndian>(self.messages)?;
        self.writer.write_u64::<LittleEndian>(offset)?;

        Ok(())
    }

    fn write_thumbnail(&mut self) -> IndexResult<u64> {
        let offset = self.writer.stream_position()?;

        let image = self
            .canvas
            .layerstack()
            .to_image(&LayerViewOptions::default());
        let image = from_dpimage(&image);

        let image = if image.width() > 256 || image.height() > 256 {
            imageops::thumbnail(&image, 256, 256)
        } else {
            image
        };

        self.writer.write_u32::<LittleEndian>(0)?; // placeholder for length
        PngEncoder::new(&mut self.writer).write_image(
            image.as_bytes(),
            image.width(),
            image.height(),
            image::ColorType::Rgba8,
        )?;

        let end_pos = self.writer.stream_position()?;
        let len = end_pos - offset - 4;
        self.writer.seek(SeekFrom::Start(offset))?;
        self.writer.write_u32::<LittleEndian>(len.try_into()?)?;

        self.writer.seek(SeekFrom::Start(end_pos))?;

        Ok(offset)
    }

    // Write a layerstack and return its offset in the index file
    fn write_layerstack(
        &mut self,
        tilemap: &mut TileMap,
        layermap: &mut LayerMap,
        annotationmap: &mut AnnotationMap,
        stats: &mut Stats,
    ) -> IndexResult<u64> {
        let ls = self.canvas.layerstack().clone(); // avoid borrow

        // First write the layers used in this layerstack
        let root_offset = self.write_grouplayer(ls.root().inner_ref(), tilemap, layermap, stats)?;

        // Write annotations
        let annotations = ls.get_annotations();
        let mut annotation_offsets: Vec<u64> = Vec::with_capacity(annotations.len());
        for annotation in annotations.iter() {
            let annotatation_p = Arc::as_ptr(annotation) as usize;
            if let Some(a) = self.last_annotations.get(&annotatation_p) {
                annotationmap.insert(annotatation_p, (a.0.clone(), a.1));
                annotation_offsets.push(a.1);
                stats.reused_annotations += 1;
            } else {
                let offset = self.write_annotation(annotation)?;
                annotationmap.insert(annotatation_p, (annotation.clone(), offset));
                annotation_offsets.push(offset);
                stats.changed_annotations += 1;
            }
        }

        // Write background tile
        let bgtile_p = tile_ptr(&ls.background);
        let bgtile_offset = if bgtile_p == 0 {
            stats.null_tiles += 1;
            0
        } else if let Some(t) = tilemap.get(&bgtile_p) {
            stats.reused_tiles += 1;
            t.1
        } else if let Some(t) = self.last_tiles.get(&bgtile_p) {
            stats.reused_tiles += 1;
            tilemap.insert(bgtile_p, t.clone());
            t.1
        } else {
            stats.new_tiles += 1;
            let tile_offset = self.write_tile(&ls.background)?;
            tilemap.insert(bgtile_p, (ls.background.clone(), tile_offset));
            tile_offset
        };

        // Write timeline if changed
        let timeline = ls.timeline();
        let timeline_ptr = Arc::as_ptr(timeline) as usize;
        if timeline_ptr != self.last_timeline.0 {
            let offset = self.writer.stream_position()?;
            self.writer
                .write_u16::<LittleEndian>(timeline.frames.len() as u16)?;
            for frame in timeline.frames.iter() {
                for l in frame.0 {
                    self.writer.write_u16::<LittleEndian>(l)?;
                }
            }

            self.last_timeline = (timeline_ptr, offset, timeline.clone());
            stats.timeline_changed = true;
        }

        // Write metadata
        if self.last_metadata.0 == 0 || self.last_metadata.1 != *ls.metadata() {
            self.last_metadata = (self.write_metadata(ls.metadata())?, ls.metadata().clone());
            stats.metadata_changed = true;
        }
        let metadata_offset = self.last_metadata.0;

        // Now we can write the actual layerstack subblock
        let offset = self.writer.stream_position()?;
        self.writer.write_u32::<LittleEndian>(ls.root().width())?;
        self.writer.write_u32::<LittleEndian>(ls.root().height())?;
        self.writer.write_u64::<LittleEndian>(bgtile_offset)?;
        self.writer
            .write_u64::<LittleEndian>(self.last_timeline.1)?;
        self.writer.write_u64::<LittleEndian>(metadata_offset)?;
        self.writer.write_u64::<LittleEndian>(root_offset)?;
        self.writer
            .write_u16::<LittleEndian>(annotations.len().try_into()?)?;
        for a in annotation_offsets {
            self.writer.write_u64::<LittleEndian>(a)?;
        }

        Ok(offset)
    }

    fn write_metadata(&mut self, md: &DocumentMetadata) -> IndexResult<u64> {
        let offset = self.writer.stream_position()?;
        self.writer.write_i32::<LittleEndian>(md.dpix)?;
        self.writer.write_i32::<LittleEndian>(md.dpiy)?;
        self.writer.write_i32::<LittleEndian>(md.framerate)?;
        Ok(offset)
    }

    fn write_annotation(&mut self, a: &Annotation) -> IndexResult<u64> {
        let offset = self.writer.stream_position()?;
        self.writer.write_u16::<LittleEndian>(a.id)?;
        self.writer.write_i32::<LittleEndian>(a.rect.x)?;
        self.writer.write_i32::<LittleEndian>(a.rect.y)?;
        self.writer.write_i32::<LittleEndian>(a.rect.w)?;
        self.writer.write_i32::<LittleEndian>(a.rect.h)?;
        self.writer
            .write_u32::<LittleEndian>(a.background.as_argb32())?;
        self.writer.write_u8(a.valign.into())?;
        self.writer
            .write_u16::<LittleEndian>(a.text.len().try_into()?)?;
        self.writer.write_all(a.text.as_bytes())?;
        Ok(offset)
    }

    fn write_grouplayer(
        &mut self,
        group: &GroupLayer,
        tilemap: &mut TileMap,
        layermap: &mut LayerMap,
        stats: &mut Stats,
    ) -> IndexResult<u64> {
        let mut layers: Vec<u64> = Vec::new();

        // First, write the group's layers
        for grouplayer in group.layervec().iter() {
            let layer_p = Arc::as_ptr(grouplayer) as usize;

            if let Some((_, offset)) = self.last_layers.get(&layer_p) {
                // If layer is unchanged, reuse previous. Copy
                // tile offsets too, so that we don't have to rewrite
                // the whole layer when something changes.
                stats.reused_layers += 1;
                layermap.insert(layer_p, (grouplayer.clone(), *offset));
                if let Layer::Bitmap(b) = grouplayer.as_ref() {
                    copy_last_tile_offsets(b, &self.last_tiles, tilemap);
                }
                layers.push(*offset);
            } else {
                // Layer has changed! Layer's content is still likely
                // *mostly* unchanged, so we can make use of last_tiles.
                stats.changed_layers += 1;

                let offset = match grouplayer.as_ref() {
                    Layer::Group(g) => self.write_grouplayer(g, tilemap, layermap, stats)?,
                    Layer::Bitmap(b) => self.write_bitmaplayer(b, tilemap, stats)?,
                };

                layermap.insert(layer_p, (grouplayer.clone(), offset));
                layers.push(offset);
            }
        }

        // Write layer subblock
        let offset = self.writer.stream_position()?;
        self.write_layer_metadata(group.metadata())?;
        self.writer.write_u8(1)?; // this is a group

        self.writer
            .write_u16::<LittleEndian>(layers.len().try_into()?)?;
        for l in layers {
            self.writer.write_u64::<LittleEndian>(l)?;
        }

        Ok(offset)
    }

    fn write_bitmaplayer(
        &mut self,
        layer: &BitmapLayer,
        tilemap: &mut TileMap,
        stats: &mut Stats,
    ) -> IndexResult<u64> {
        let mut sublayers: Vec<u64> = Vec::new();
        let mut tiles: Vec<u64> = Vec::with_capacity(layer.tilevec().len());

        // First, write the sublayers (if any)
        // Sublayers are very short lived, so we don't bother trying to reuse them
        for sublayer in layer
            .sublayervec()
            .iter()
            .filter(|sl| sl.metadata().is_visible() && sl.metadata().id < 256)
        {
            stats.changed_layers += 1;
            let offset = self.write_bitmaplayer(sublayer, tilemap, stats)?;
            sublayers.push(offset);
        }

        // Write the changed tiles
        for tile in layer.tilevec() {
            let tile_p = tile_ptr(tile);
            if tile_p == 0 {
                stats.null_tiles += 1;
                tiles.push(0);
            } else if let Some(t) = tilemap.get(&tile_p) {
                stats.reused_tiles += 1;
                tiles.push(t.1);
            } else if let Some(t) = self.last_tiles.get(&tile_p) {
                stats.reused_tiles += 1;
                tilemap.insert(tile_p, t.clone());
                tiles.push(t.1);
            } else {
                stats.new_tiles += 1;
                let tile_offset = self.write_tile(tile)?;
                tilemap.insert(tile_p, (tile.clone(), tile_offset));
                tiles.push(tile_offset);
            }
        }

        // Write layer subblock
        let offset = self.writer.stream_position()?;
        self.write_layer_metadata(layer.metadata())?;
        self.writer.write_u8(0)?; // not a group

        self.writer
            .write_u16::<LittleEndian>(sublayers.len().try_into()?)?;
        for sl in sublayers {
            self.writer.write_u64::<LittleEndian>(sl)?;
        }

        for t in tiles {
            self.writer.write_u64::<LittleEndian>(t)?;
        }

        Ok(offset)
    }

    fn write_layer_metadata(&mut self, metadata: &LayerMetadata) -> IndexResult<()> {
        self.writer.write_u16::<LittleEndian>(metadata.id)?;
        self.writer
            .write_u16::<LittleEndian>(metadata.title.len().try_into()?)?;
        self.writer.write_all(metadata.title.as_bytes())?;
        self.writer.write_u8((metadata.opacity * 255.0) as u8)?;
        self.writer.write_u8(metadata.blendmode.into())?;
        self.writer.write_u8(metadata.hidden as u8)?;
        self.writer.write_u8(metadata.censored as u8)?;
        self.writer.write_u8(metadata.isolated as u8)?;

        Ok(())
    }

    fn write_tile(&mut self, tile: &Tile) -> IndexResult<u64> {
        let data = compress_tile(tile);

        let offset = self.writer.stream_position()?;
        self.writer
            .write_u16::<LittleEndian>(data.len().try_into()?)?;
        self.writer.write_all(&data)?;
        Ok(offset)
    }
}

/// Remember the last used tiles of this layer in the next generation.
fn copy_last_tile_offsets(layer: &BitmapLayer, last_tiles: &TileMap, new_tiles: &mut TileMap) {
    for t in layer.tilevec() {
        // Note: this function is used to pass along the tile data offsets of the
        // previously written layer. Therefore, it is safe to assume that
        // the offsets are all found in last_tiles.
        let tile_p = tile_ptr(t);

        // Blank tiles are not written at all
        if tile_p > 0 {
            let (_, offset) = last_tiles.get(&tile_p).unwrap();
            new_tiles.insert(tile_p, (t.clone(), *offset));
        }
    }
}

fn tile_ptr(t: &Tile) -> usize {
    match t {
        Tile::Blank => 0,
        Tile::Bitmap(td) => Arc::as_ptr(td) as usize,
    }
}
