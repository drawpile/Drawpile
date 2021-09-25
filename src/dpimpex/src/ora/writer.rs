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

use crate::conv::from_dpimage;
use crate::{ImageExportResult, ImpexError};
use super::{DP_NAMESPACE, MYPAINT_NAMESPACE,
    OraCommon, Isolation, OraStackElement, OraStack, OraLayer};
use dpcore::paint::annotation::VAlign;
use dpcore::paint::tile::TILE_SIZEI;
use dpcore::paint::{
    Blendmode, Image,
    LayerStack, LayerViewOptions,
    Rectangle,
    Layer, GroupLayer, BitmapLayer
};

use image::codecs::png::PngEncoder;
use image::{imageops, ColorType, EncodableLayout, ImageEncoder, RgbaImage};
use std::fs::File;
use std::io::{Seek, Write};
use std::path::Path;
use xml::common::XmlVersion;
use xml::writer::{XmlEvent, EventWriter};
use xml::writer::events::StartElementBuilder;
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
    let mut written_layers = write_stack(&mut archive, layerstack.root().inner_ref())?;

    if !layerstack.background.is_blank() {
        let bg = write_background(&mut archive, &layerstack)?;

        written_layers.layers.push(OraStackElement::Layer(bg));
    }

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

fn write_background<W: Write + Seek>(
    archive: &mut ZipWriter<W>,
    layerstack: &LayerStack,
) -> Result<OraLayer, ImpexError> {
    let bgl = BitmapLayer::new(
        0,
        layerstack.root().width(),
        layerstack.root().height(),
        layerstack.background.clone(),
    );

    let filename = String::from("data/background.png");
    let tilename = String::from("data/background-tile.png");
    write_png(
        archive,
        &filename,
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
    write_png(archive, &tilename, &from_dpimage(&bgt))?;

    Ok(OraLayer{
        common: OraCommon {
            name: "Background".into(),
            offset: (0, 0),
            opacity: 1.0,
            visibility: true,
            locked: false,
            censored: false,
            fixed: false,
            composite_op: Blendmode::Normal,
            unsupported_features: false
        },
        filename,
        bgtile: tilename,
    })
}

fn write_layer<W: Write + Seek>(
    archive: &mut ZipWriter<W>,
    layer: &BitmapLayer,
) -> Result<OraLayer, ImpexError> {
    let (image, x, y) = layer.to_cropped_image();
    let md = layer.metadata();

    let ol = OraLayer {
        common: OraCommon {
            name: md.title.clone(),
            offset: (x, y),
            opacity: md.opacity,
            visibility: !md.hidden,
            locked: false, // TODO
            censored: md.censored,
            fixed: md.fixed,
            composite_op: md.blendmode,
            unsupported_features: false,
        },
        filename: format!("data/layer-{:04x}.png", layer.metadata().id),
        bgtile: String::new(),
    };

    if image.width > 0 {
        write_png(archive, &ol.filename, &from_dpimage(&image))?;

    } else {
        let blank = layer.to_image(Rectangle::new(0, 0, layer.width() as i32, layer.height() as i32)).unwrap();
        write_png(archive, &ol.filename, &from_dpimage(&blank))?;
    }

    Ok(ol)
}

fn write_stack<W: Write + Seek>(
    archive: &mut ZipWriter<W>,
    group: &GroupLayer,
) -> Result<OraStack, ImpexError> {
    let md = group.metadata();
    let mut stack = OraStack {
        common: OraCommon {
            name: md.title.clone(),
            offset: (0, 0),
            opacity: md.opacity,
            visibility: !md.hidden,
            locked: false, // TODO
            censored: md.censored,
            fixed: md.fixed,
            composite_op: md.blendmode,
            unsupported_features: false,
        },
        isolation: if group.metadata().isolated { Isolation::Isolate } else { Isolation::Auto },
        layers: Vec::new(),
        annotations: Vec::new(), // unused here
    };

    for layer in group.iter_layers() {
        match layer {
            Layer::Bitmap(b) => {
                stack.layers.push(OraStackElement::Layer(write_layer(archive, b)?));
            }
            Layer::Group(g) => {
                stack.layers.push(OraStackElement::Stack(write_stack(archive, g)?));
            }
        };
    }

    Ok(stack)
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
    root: &OraStack,
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
        let w = layerstack.root().width().to_string();
        let h = layerstack.root().height().to_string();
        let e = XmlEvent::start_element("image")
            .ns("mypaint", MYPAINT_NAMESPACE)
            .ns("drawpile", DP_NAMESPACE)
            .attr("w", &w)
            .attr("h", &h)
            .attr("version", "0.0.3");
        // TODO DPI
        writer.write(e)?;
    }

    write_stack_xml_stack(&mut writer, root)?;

    // Annotations
    // (note: prior to Drawpile 2.2, this was put inside the <stack> element)
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

        writer.write(XmlEvent::end_element().name("annotations"))?;
    }

    writer.write(XmlEvent::end_element().name("image"))?;

    Ok(())
}

fn write_stack_xml_stack<W: Write>(writer: &mut EventWriter<W>, stack: &OraStack) -> ImageExportResult {
    // Root stack
    let opacity = format!("{:.4}", stack.common.opacity);
    let mut stack_element = XmlEvent::start_element("stack")
        .attr("opacity", &opacity);

    stack_element = stack_common_attrs(stack_element, &stack.common);

    if matches!(stack.isolation, Isolation::Isolate) {
        stack_element = stack_element.attr("isolation", "isolate");
    }

    writer.write(stack_element)?;
    for wl in &stack.layers {
        match wl {
            OraStackElement::Stack(s) => {
                write_stack_xml_stack(writer, s)?;
            }
            OraStackElement::Layer(l) => {
                // we don't have offsets for stacks
                let x = l.common.offset.0.to_string();
                let y = l.common.offset.1.to_string();
                let opacity = format!("{:.4}", l.common.opacity);

                let mut element = XmlEvent::start_element(wl.element_name())
                    .attr("x", &x)
                    .attr("y", &y)
                    .attr("opacity", &opacity);

                element = stack_common_attrs(element, &l.common);
                element = element.attr("src", &l.filename);
                if !l.bgtile.is_empty() {
                    element = element.attr("mypaint:background-tile", &l.bgtile)
                }

                writer.write(element)?;
                writer.write(XmlEvent::end_element())?;
            }
        }
    }
    writer.write(XmlEvent::end_element())?; // /stack

    Ok(())
}

fn stack_common_attrs<'a>(mut el: StartElementBuilder<'a>, common: &'a OraCommon) -> StartElementBuilder<'a> {
    el = el.attr("name", &common.name);

    if !common.visibility {
        el = el.attr("visibility", "hidden");
    }
    if common.composite_op != Blendmode::Normal {
        el = el.attr("composite-op", common.composite_op.svg_name());
    }
    if common.censored {
        el = el.attr("drawpile:censored", "true")
    }
    if common.fixed {
        el = el.attr("drawpile:fixed", "true")
    }
    if common.locked {
        el = el.attr("edit-locked", "true");
    }

    el
}
