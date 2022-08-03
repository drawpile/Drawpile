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

use super::{IndexEntry, IndexError, IndexResult, INDEX_FORMAT_VERSION};
use dpcore::canvas::compression::decompress_tile;
use dpcore::paint::annotation::{Annotation, VAlign};
use dpcore::paint::{
    BitmapLayer, Blendmode, Color, DocumentMetadata, Frame, GroupLayer, Layer, LayerMetadata,
    LayerStack, Rectangle, Tile, Timeline,
};

use byteorder::{LittleEndian, ReadBytesExt};
use std::collections::HashMap;
use std::convert::TryFrom;
use std::io::{Read, Seek, SeekFrom};
use std::sync::Arc;

pub struct Index {
    pub messages: u32,
    pub entries: Vec<IndexEntry>,
}

pub fn read_index<R: Read + Seek>(reader: &mut R) -> IndexResult<Index> {
    let mut magic: Vec<u8> = vec![0; 6];

    reader.read_exact(&mut magic)?;

    if magic != b"DPIDX\0" {
        return Err(IndexError::IncompatibleVersion);
    }

    let version = reader.read_u16::<LittleEndian>()?;
    if version != INDEX_FORMAT_VERSION {
        return Err(IndexError::IncompatibleVersion);
    }

    let messages = reader.read_u32::<LittleEndian>()?;
    let index_offset = reader.read_u64::<LittleEndian>()?;

    reader.seek(SeekFrom::Start(index_offset))?;
    let mut entries = Vec::new();

    loop {
        // Read message index. EOF is an expected result here.
        let message_index = match reader.read_u32::<LittleEndian>() {
            Ok(i) => i,
            Err(e) => {
                if matches!(e.kind(), std::io::ErrorKind::UnexpectedEof) {
                    break;
                } else {
                    return Err(e.into());
                }
            }
        };

        let message_offset = reader.read_u64::<LittleEndian>()?;
        let snapshot_offset = reader.read_u64::<LittleEndian>()?;
        let thumbnail_offset = reader.read_u64::<LittleEndian>()?;

        entries.push(IndexEntry {
            message_index,
            message_offset,
            snapshot_offset,
            thumbnail_offset,
        });
    }

    Ok(Index { messages, entries })
}

/// Read a snapshot from the index
///
/// Returns message index, message offset and the layer stack.
pub fn read_snapshot<R: Read + Seek>(
    reader: &mut R,
    entry: &IndexEntry,
) -> IndexResult<LayerStack> {
    reader.seek(SeekFrom::Start(entry.snapshot_offset))?;
    let width = reader.read_u32::<LittleEndian>()?;
    let height = reader.read_u32::<LittleEndian>()?;
    let bgtile_offset = reader.read_u64::<LittleEndian>()?;
    let timeline_offset = reader.read_u64::<LittleEndian>()?;
    let metadata_offset = reader.read_u64::<LittleEndian>()?;
    let root_offset = reader.read_u64::<LittleEndian>()?;
    let annotation_count = reader.read_u16::<LittleEndian>()?;
    let annotation_offsets = read_offset_vector(reader, annotation_count as usize)?;

    let background = read_tile(reader, &mut HashMap::new(), bgtile_offset)?;
    let timeline = read_timeline(reader, timeline_offset)?;
    let metadata = read_metadata(reader, metadata_offset)?;

    let root = match read_layer(reader, root_offset, width, height, false) {
        Ok(Layer::Group(g)) => g,
        Ok(_) => {
            return Err(IndexError::CorruptFile);
        }
        Err(e) => {
            return Err(e);
        }
    };

    let mut annotations: Vec<Arc<Annotation>> = Vec::with_capacity(annotation_count as usize);
    for offset in annotation_offsets {
        annotations.push(read_annotation(reader, offset)?);
    }

    // TODO caching for annotations, layers and metadata
    Ok(LayerStack::from_parts(
        Arc::new(root.into()),
        Arc::new(annotations),
        Arc::new(metadata),
        Arc::new(timeline),
        background,
    ))
}

pub fn read_metadata<R: Read + Seek>(reader: &mut R, offset: u64) -> IndexResult<DocumentMetadata> {
    reader.seek(SeekFrom::Start(offset))?;
    let dpix = reader.read_i32::<LittleEndian>()?;
    let dpiy = reader.read_i32::<LittleEndian>()?;
    let framerate = reader.read_i32::<LittleEndian>()?;
    let use_timeline = false; // FIXME
    Ok(DocumentMetadata {
        dpix,
        dpiy,
        framerate,
        use_timeline,
    })
}

pub fn read_timeline<R: Read + Seek>(reader: &mut R, offset: u64) -> IndexResult<Timeline> {
    reader.seek(SeekFrom::Start(offset))?;
    let count = reader.read_u16::<LittleEndian>()?;
    let mut frames: Vec<Frame> = Vec::with_capacity(count as usize);
    for _ in 0..count {
        let mut f = Frame::empty();
        for l in 0..f.0.len() {
            f.0[l] = reader.read_u16::<LittleEndian>()?;
        }
        frames.push(f);
    }

    Ok(Timeline { frames })
}

pub fn read_thumbnail<R: Read + Seek>(reader: &mut R, offset: u64) -> IndexResult<Vec<u8>> {
    reader.seek(SeekFrom::Start(offset))?;
    let len = reader.read_u32::<LittleEndian>()?;
    let mut img = vec![0; len as usize];
    reader.read_exact(&mut img)?;

    Ok(img)
}

fn read_annotation<R: Read + Seek>(reader: &mut R, offset: u64) -> IndexResult<Arc<Annotation>> {
    reader.seek(SeekFrom::Start(offset))?;

    let id = reader.read_u16::<LittleEndian>()?;
    let x = reader.read_i32::<LittleEndian>()?;
    let y = reader.read_i32::<LittleEndian>()?;
    let w = reader.read_i32::<LittleEndian>()?;
    let h = reader.read_i32::<LittleEndian>()?;
    let bg = reader.read_u32::<LittleEndian>()?;
    let valign = VAlign::try_from(reader.read_u8()?).unwrap_or(VAlign::Top);
    let text_len = reader.read_u16::<LittleEndian>()?;
    let mut text = vec![0; text_len as usize];
    reader.read_exact(&mut text)?;
    let text = String::from_utf8(text)?;

    Ok(Arc::new(Annotation {
        id,
        text,
        rect: Rectangle::new(x, y, w, h),
        background: Color::from_argb32(bg),
        protect: false,
        valign,
        border: 0, // border not fully implemented yet
    }))
}

fn read_layer<R: Read + Seek>(
    reader: &mut R,
    offset: u64,
    width: u32,
    height: u32,
    read_sublayers: bool,
) -> IndexResult<Layer> {
    let mut tilemap: HashMap<u64, Tile> = HashMap::new();

    reader.seek(SeekFrom::Start(offset))?;

    let id = reader.read_u16::<LittleEndian>()?;
    let title_len = reader.read_u16::<LittleEndian>()?;
    let mut title = vec![0; title_len as usize];
    reader.read_exact(&mut title)?;
    let title = String::from_utf8(title)?;
    let opacity = reader.read_u8()? as f32 / 255.0;
    let blendmode = Blendmode::try_from(reader.read_u8()?).unwrap_or_default();
    let hidden = reader.read_u8()? != 0;
    let censored = reader.read_u8()? != 0;
    let isolated = reader.read_u8()? != 0;
    let group = reader.read_u8()? != 0;

    let metadata = LayerMetadata {
        id,
        title,
        opacity,
        hidden,
        censored,
        blendmode,
        isolated,
    };

    let sublayer_count = reader.read_u16::<LittleEndian>()?;
    let sublayer_offsets = read_offset_vector(reader, sublayer_count as usize)?;

    Ok(if group {
        // Read group layer
        // TODO cycle check to avoid infinite loops
        let mut layers: Vec<Arc<Layer>> = Vec::with_capacity(sublayer_count as usize);
        for sl in sublayer_offsets {
            layers.push(Arc::new(read_layer(reader, sl, width, height, true)?));
        }

        Layer::Group(GroupLayer::from_parts(metadata, width, height, layers))
    } else {
        // Read layer
        let tile_count = Tile::div_up(width) * Tile::div_up(height);
        let tile_offsets = read_offset_vector(reader, tile_count as usize)?;

        let mut tiles: Vec<Tile> = Vec::with_capacity(tile_offsets.len());
        for offset in tile_offsets {
            tiles.push(read_tile(reader, &mut tilemap, offset)?);
        }

        let mut sublayers: Vec<Arc<BitmapLayer>> = Vec::with_capacity(sublayer_count as usize);
        if read_sublayers {
            for sl in sublayer_offsets {
                if let Layer::Bitmap(b) = read_layer(reader, sl, width, height, false)? {
                    sublayers.push(Arc::new(b))
                } else {
                    return Err(IndexError::CorruptFile);
                }
            }
        }

        Layer::Bitmap(BitmapLayer::from_parts(
            metadata,
            width,
            height,
            Arc::new(tiles),
            sublayers,
        ))
    })
}

fn read_tile<R: Read + Seek>(
    reader: &mut R,
    tile_cache: &mut HashMap<u64, Tile>,
    offset: u64,
) -> IndexResult<Tile> {
    if offset == 0 {
        Ok(Tile::Blank)
    } else if let Some(t) = tile_cache.get(&offset) {
        Ok(t.clone())
    } else {
        reader.seek(SeekFrom::Start(offset))?;
        let data_len = reader.read_u16::<LittleEndian>()?;
        let mut data = vec![0; data_len as usize];
        reader.read_exact(&mut data)?;
        let tile = decompress_tile(&data, 0).ok_or(IndexError::BadTile)?;
        tile_cache.insert(offset, tile.clone());
        Ok(tile)
    }
}

fn read_offset_vector<R: Read>(r: &mut R, count: usize) -> IndexResult<Vec<u64>> {
    let mut v: Vec<u64> = Vec::with_capacity(count);
    for _ in 0..count {
        v.push(r.read_u64::<LittleEndian>()?);
    }

    Ok(v)
}
