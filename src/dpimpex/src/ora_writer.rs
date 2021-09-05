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

use super::conv::from_dpimage;
use super::ora_utils::{DP_NAMESPACE, MYPAINT_NAMESPACE};
use super::{ImageExportResult, ImpexError};
use dpcore::paint::annotation::VAlign;
use dpcore::paint::tile::TILE_SIZEI;
use dpcore::paint::{
    Blendmode, Image, InternalLayerID, Layer, LayerID, LayerStack, LayerViewOptions, Rectangle,
};

use image::codecs::png::PngEncoder;
use image::{imageops, ColorType, EncodableLayout, ImageEncoder, RgbaImage};
use std::convert::TryInto;
use std::fs::File;
use std::io::{Seek, Write};
use std::path::Path;
use xml::common::XmlVersion;
use xml::writer::XmlEvent;
use xml::EmitterConfig;
use zip::ZipWriter;

pub fn save_openraster_image(path: &Path, layerstack: &LayerStack) -> ImageExportResult {
    let mut archive = ZipWriter::new(File::create(path)?);

    // Write the mimetype file. This must be the first file in the archive,
    // and must be STORED, not DEFLATED.
    archive.start_file(
        "mimetype",
        zip::write::FileOptions::default().compression_method(zip::CompressionMethod::Stored),
    )?;
    archive.write(b"image/openraster")?;

    // Write layer images. We need to do this first because the layers are automatically
    // cropped and we need the offsets in stack.xml.
    let written_layers = write_layers(&mut archive, layerstack)?;

    // Write the merged image and thumbnail
    {
        let flattened = from_dpimage(&layerstack.to_image(&LayerViewOptions::default()));
        write_png(&mut archive, "mergedimage.png", &flattened)?;
        let thumb = imageops::thumbnail(&flattened, 256, 256);
        write_png(&mut archive, "Thumbnails/thumbnail.png", &thumb)?;
    }

    // Write the stack XML
    write_stack_xml(&mut archive, &written_layers, &layerstack)
}

struct WrittenLayer {
    filename: String,
    bg_tile: String, // set only for the background layer (if any)
    offset: (i32, i32),
    id: LayerID, // will be zero for the background layer (if it exists)
}

fn write_layers<W: Write + Seek>(
    archive: &mut ZipWriter<W>,
    layerstack: &LayerStack,
) -> Result<Vec<WrittenLayer>, ImpexError> {
    let mut written = Vec::new();

    // Write background layer
    if !layerstack.background.is_blank() {
        let wl = WrittenLayer {
            filename: "data/background.png".into(),
            bg_tile: "data/background-tile.png".into(),
            offset: (0, 0),
            id: 0,
        };

        let bgl = Layer::new(
            InternalLayerID(0),
            layerstack.width(),
            layerstack.height(),
            layerstack.background.clone(),
        );
        write_png(
            archive,
            &wl.filename,
            &from_dpimage(
                &bgl.to_image(Rectangle::new(
                    0,
                    0,
                    bgl.width() as i32,
                    bgl.height() as i32,
                ))
                .unwrap(),
            ),
        )?;

        let mut bgt = Image::new(TILE_SIZEI as usize, TILE_SIZEI as usize);
        bgl.to_pixels(Rectangle::tile(0, 0, TILE_SIZEI), &mut bgt.pixels)
            .unwrap();
        write_png(archive, &wl.bg_tile, &from_dpimage(&bgt))?;

        written.push(wl);
    }

    // Write the actual layers
    let mut had_blank_layers = false;

    for (i, layer) in layerstack.iter_layers().enumerate() {
        let (image, x, y) = layer.to_cropped_image();

        let wl = WrittenLayer {
            filename: if image.width == 0 {
                had_blank_layers = true;
                "data/blank.png".into()
            } else {
                format!("data/layer-{}-{:04x}.png", i, layer.id.0)
            },
            bg_tile: String::new(),
            offset: (x, y),
            id: layer.id.try_into().unwrap(),
        };

        if image.width > 0 {
            write_png(archive, &wl.filename, &from_dpimage(&image))?;
        }

        written.push(wl);
    }

    // A dummy image for blank layers
    if had_blank_layers {
        let blank = RgbaImage::from_raw(64, 64, vec![0; 64 * 64 * 4]).unwrap();
        write_png(archive, "data/blank.png", &blank)?;
    }

    // In OpenRaster, the layers are listed from top-to-bottom,
    // while in Drawpile they're stored bottom-to-top.
    written.reverse();

    Ok(written)
}

fn write_png<W: Write + Seek>(
    archive: &mut ZipWriter<W>,
    filename: &str,
    image: &RgbaImage,
) -> Result<(), ImpexError> {
    let store_options =
        zip::write::FileOptions::default().compression_method(zip::CompressionMethod::Stored);
    archive.start_file(filename, store_options)?;

    PngEncoder::new(archive).write_image(
        image.as_bytes(),
        image.width(),
        image.height(),
        ColorType::Rgba8,
    )?;

    Ok(())
}

fn write_stack_xml<W: Write + Seek>(
    archive: &mut ZipWriter<W>,
    written_layers: &[WrittenLayer],
    layerstack: &LayerStack,
) -> ImageExportResult {
    archive.start_file("stack.xml", zip::write::FileOptions::default())?;
    let mut writer = EmitterConfig::new()
        .perform_indent(true)
        .create_writer(archive);

    writer.write(XmlEvent::StartDocument {
        version: XmlVersion::Version10,
        encoding: None,
        standalone: None,
    })?;

    // Document root
    {
        let w = layerstack.width().to_string();
        let h = layerstack.height().to_string();
        let e = XmlEvent::start_element("image")
            .ns("mypaint", MYPAINT_NAMESPACE)
            .ns("drawpile", DP_NAMESPACE)
            .attr("w", &w)
            .attr("h", &h)
            .attr("version", "0.0.3");
        // TODO DPI
        writer.write(e)?;
    }

    // Root stack
    writer.write(XmlEvent::start_element("stack"))?;

    // Annotations
    if !layerstack.get_annotations().is_empty() {
        writer.write(XmlEvent::start_element("drawpile:annotations"))?;

        for a in layerstack.get_annotations().iter() {
            let x = a.rect.x.to_string();
            let y = a.rect.y.to_string();
            let w = a.rect.w.to_string();
            let h = a.rect.h.to_string();
            let bg = a.background.to_string();
            let mut e = XmlEvent::start_element("drawpile:a")
                .attr("x", &x)
                .attr("y", &y)
                .attr("w", &w)
                .attr("h", &h)
                .attr("bg", &bg);

            match a.valign {
                VAlign::Top => (),
                VAlign::Center => {
                    e = e.attr("valign", "center");
                }
                VAlign::Bottom => {
                    e = e.attr("valign", "bottom");
                }
            };

            writer.write(e)?;
            writer.write(XmlEvent::CData(&a.text))?;
            writer.write(XmlEvent::end_element())?;
        }

        writer.write(XmlEvent::end_element())?; // image/stack/annotations
    }

    // Layers
    for wl in written_layers {
        let layer = layerstack.get_layer(wl.id);
        if let Some(layer) = layer {
            let x = wl.offset.0.to_string();
            let y = wl.offset.1.to_string();

            let opacity = format!("{:.4}", layer.opacity);
            let mut e = XmlEvent::start_element("layer")
                .attr("src", &wl.filename)
                .attr("name", &layer.title)
                .attr("opacity", &opacity)
                .attr("x", &x)
                .attr("y", &y);

            if layer.hidden {
                e = e.attr("visibility", "hidden");
            }
            if layer.blendmode != Blendmode::Normal {
                e = e.attr("blendmode", layer.blendmode.svg_name());
            }
            if layer.censored {
                e = e.attr("drawpile:censored", "true")
            }
            if layer.fixed {
                e = e.attr("drawpile:fixed", "true")
            }
            writer.write(e)?;
            writer.write(XmlEvent::end_element())?;
        } else {
            // must be the background layer
            assert!(!wl.bg_tile.is_empty());
            writer.write(
                XmlEvent::start_element("layer")
                    .attr("src", &wl.filename)
                    .attr("mypaint:background-tile", &wl.bg_tile)
                    .attr("name", "Background"),
            )?;
            writer.write(XmlEvent::end_element())?;
        }
    }

    writer.write(XmlEvent::end_element())?; // image/stack
    writer.write(XmlEvent::end_element())?; // image

    Ok(())
}
