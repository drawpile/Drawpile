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

use super::{
    Isolation, OraAnnotation, OraCanvas, OraCommon, OraLayer, OraStack, OraStackElement,
    OraTimeline, DP_NAMESPACE, MYPAINT_NAMESPACE,
};
use crate::conv::to_dpimage;
use crate::{ImageImportResult, ImpexError};

use dpcore::paint::annotation::VAlign;
use dpcore::paint::{
    editlayer, Blendmode, Color, Frame, Image8, LayerID, LayerInsertion, LayerStack, Rectangle,
    Size, Tile,
};

use std::fs::File;
use std::io::{Cursor, Read, Seek};
use std::path::Path;
use std::str::FromStr;

use image::io::Reader as ImageReader;
use tracing::warn;
use xml::attribute::OwnedAttribute;
use xml::reader::{EventReader, XmlEvent};
use zip::ZipArchive;

pub fn load_openraster_image(path: &Path) -> ImageImportResult {
    let mut archive = ZipArchive::new(File::open(path)?)?;

    check_mimetype(&mut archive)?;
    let canvas = parse_stack_xml(archive.by_name("stack.xml")?)?;

    let mut ls = LayerStack::new(canvas.size.width as u32, canvas.size.height as u32);

    ls.metadata_mut().dpix = canvas.dpi.0;
    ls.metadata_mut().dpiy = canvas.dpi.1;
    ls.metadata_mut().framerate = canvas.framerate;

    // Create layers
    create_stack(&mut archive, &mut ls, 0, &canvas.root)?;

    // Create annotations
    let mut annotation_id = 0x0100;
    for ora_ann in canvas.annotations {
        ls.add_annotation(annotation_id, ora_ann.rect);
        let mut a = ls.get_annotation_mut(annotation_id).unwrap();
        a.text = ora_ann.content;
        a.background = Color::from_str(&ora_ann.bg).unwrap_or(Color::TRANSPARENT);
        a.valign = match ora_ann.valign.as_ref() {
            "center" => VAlign::Center,
            "bottom" => VAlign::Bottom,
            _ => VAlign::Top,
        };
        annotation_id += 1;
    }

    // Set timeline
    ls.metadata_mut().use_timeline = canvas.timeline.enabled;
    if !canvas.timeline.frames.is_empty() {
        let layer_ids = make_idlist(&canvas.root);
        let lst = ls.timeline_mut();
        for frame in canvas.timeline.frames.iter() {
            let mut f = Frame::empty();
            frame
                .iter()
                .filter_map(|&layer_idx| layer_ids.get(layer_idx as usize))
                .take(f.0.len())
                .enumerate()
                .for_each(|(i, &l)| f.0[i] = l);
            lst.frames.push(f);
        }
    }

    // TODO warn about unsupported features
    Ok(ls)
}

fn create_stack<R: Read + Seek>(
    archive: &mut ZipArchive<R>,
    layerstack: &mut LayerStack,
    parent_id: LayerID,
    stack: &OraStack,
) -> Result<(), ImpexError> {
    for layer in stack.layers.iter().rev() {
        match layer {
            OraStackElement::Stack(substack) => {
                let subgroup = layerstack
                    .root_mut()
                    .add_group_layer(substack.common.id, LayerInsertion::Into(parent_id))
                    .unwrap()
                    .as_group_mut()
                    .unwrap();
                let metadata = subgroup.metadata_mut();

                metadata.title = substack.common.name.clone();
                metadata.opacity = substack.common.opacity.clamp(0.0, 1.0);
                metadata.hidden = !substack.common.visibility;
                metadata.censored = substack.common.censored;
                metadata.blendmode = substack.common.composite_op;
                metadata.isolated = matches!(substack.isolation, Isolation::Isolate);

                create_stack(archive, layerstack, substack.common.id, substack)?;
            }
            OraStackElement::Layer(oralayer) => {
                if !oralayer.bgtile.is_empty() {
                    let tile_image = get_image_file(archive, &oralayer.bgtile)?;
                    if tile_image.width == 64 || tile_image.height == 64 {
                        // If we have a valid tile, this is a background layer
                        // that we don't need to represent as an actual layer
                        layerstack.background = Tile::from_data(&tile_image.pixels, 0);
                        continue;
                    }
                }

                let img = get_image_file(archive, &oralayer.filename)?;
                let layer = layerstack
                    .root_mut()
                    .add_bitmap_layer(
                        oralayer.common.id,
                        Color::TRANSPARENT,
                        LayerInsertion::Into(parent_id),
                    )
                    .unwrap()
                    .as_bitmap_mut()
                    .unwrap();

                let mut metadata = layer.metadata_mut();
                metadata.title = oralayer.common.name.clone();
                metadata.opacity = oralayer.common.opacity.clamp(0.0, 1.0);
                metadata.hidden = !oralayer.common.visibility;
                metadata.censored = oralayer.common.censored;
                metadata.blendmode = oralayer.common.composite_op;

                // TODO layer lock status

                editlayer::draw_image8(
                    layer,
                    1,
                    &img.pixels,
                    &Rectangle::new(
                        oralayer.common.offset.0,
                        oralayer.common.offset.1,
                        img.width as i32,
                        img.height as i32,
                    ),
                    1.0,
                    Blendmode::Replace,
                );
            }
        }
    }

    Ok(())
}

/// OpenRaster files are identified by the presence of a "mimetype" file
fn check_mimetype<R: Read + Seek>(archive: &mut ZipArchive<R>) -> Result<(), ImpexError> {
    let expected_mimetype = b"image/openraster";

    let mut mtfile = archive.by_name("mimetype")?;
    if mtfile.size() != expected_mimetype.len() as u64 {
        return Err(ImpexError::UnsupportedFormat);
    }

    let mut mimetype = Vec::new();
    mtfile.read_to_end(&mut mimetype)?;

    if mimetype == expected_mimetype {
        Ok(())
    } else {
        Err(ImpexError::UnsupportedFormat)
    }
}

fn get_image_file<R: Read + Seek>(
    archive: &mut ZipArchive<R>,
    filename: &str,
) -> Result<Image8, ImpexError> {
    let mut filecontent = Vec::new();
    archive.by_name(filename)?.read_to_end(&mut filecontent)?;
    let img = ImageReader::new(Cursor::new(filecontent))
        .with_guessed_format()?
        .decode()?;

    Ok(to_dpimage(&img.into_rgba8()))
}

fn parse_stack_xml<R: Read>(file: R) -> Result<OraCanvas, ImpexError> {
    let mut parser = EventReader::new(file);

    // Expect <image> root element
    loop {
        match parser.next() {
            Ok(XmlEvent::StartElement {
                name, attributes, ..
            }) => {
                if name.local_name != "image" {
                    warn!(
                        "Error reading OpenRaster stack: expected <image>, got <{}>",
                        name
                    );
                    return Err(ImpexError::UnsupportedFormat);
                }

                return parse_stack_image(attributes, &mut parser);
            }
            Ok(XmlEvent::EndDocument) => {
                warn!("Error reading OpenRaster stack: Unexpected end of document");
                return Err(ImpexError::UnsupportedFormat);
            }
            Err(e) => {
                warn!("Error reading OpenRaster stack: {}", e);
                return Err(ImpexError::UnsupportedFormat);
            }
            _ => {}
        }
    }
}

fn parse_stack_image<R: Read>(
    mut attributes: Vec<OwnedAttribute>,
    parser: &mut EventReader<R>,
) -> Result<OraCanvas, ImpexError> {
    let mut canvas = OraCanvas {
        size: Size::new(
            take_attribute(&mut attributes, "w", None)
                .and_then(|a| a.parse::<i32>().ok())
                .unwrap_or(0),
            take_attribute(&mut attributes, "h", None)
                .and_then(|a| a.parse::<i32>().ok())
                .unwrap_or(0),
        ),
        unsupported_features: false,
        dpi: (
            take_attribute(&mut attributes, "xres", None)
                .and_then(|a| a.parse::<i32>().ok())
                .unwrap_or(72),
            take_attribute(&mut attributes, "yres", None)
                .and_then(|a| a.parse::<i32>().ok())
                .unwrap_or(72),
        ),
        framerate: take_attribute(&mut attributes, "framerate", None)
            .and_then(|a| a.parse::<i32>().ok())
            .unwrap_or(24),
        root: OraStack {
            common: OraCommon {
                name: String::new(),
                offset: (0, 0),
                opacity: 1.0,
                visibility: true,
                locked: false,
                censored: false,
                composite_op: Blendmode::Normal,
                unsupported_features: false,
                id: 0, // TODO
            },
            isolation: Isolation::Auto,
            layers: Vec::new(),
            annotations: Vec::new(),
        },
        annotations: Vec::new(),
        timeline: OraTimeline {
            frames: Vec::new(),
            enabled: false,
        },
    };

    if canvas.size.width <= 0 || canvas.size.height <= 0 {
        warn!("OpenRaster file has invalid image size");
        return Err(ImpexError::UnsupportedFormat);
    }
    loop {
        match parser.next() {
            Ok(XmlEvent::StartElement {
                name, attributes, ..
            }) => {
                if name.local_name == "stack" {
                    let mut next_layer_id = 0x0100;
                    canvas.root =
                        parse_stack_stack(attributes, &mut next_layer_id, (0, 0), parser)?;
                    canvas.annotations.append(&mut canvas.root.annotations);
                    canvas.unsupported_features |= canvas.root.common.unsupported_features;
                } else if name.local_name == "annotations"
                    && name.namespace.as_deref() == Some(DP_NAMESPACE)
                {
                    canvas.annotations.append(&mut parse_annotations(parser)?);
                } else if name.local_name == "timeline"
                    && name.namespace.as_deref() == Some(DP_NAMESPACE)
                {
                    canvas.timeline = parse_timeline(attributes, parser)?;
                } else {
                    warn!("Unsupported openraster <image> element <{}>", name);
                    canvas.unsupported_features = true;
                    skip_element(parser)?;
                }
            }
            Ok(XmlEvent::EndElement { name, .. }) => {
                assert_eq!(name.local_name, "image");
                return Ok(canvas);
            }
            Ok(XmlEvent::EndDocument) => {
                warn!("Error reading OpenRaster stack: Unexpected end of document while parsing <image>");
                return Err(ImpexError::UnsupportedFormat);
            }
            Err(e) => {
                warn!("Error reading OpenRaster stack: {}", e);
                return Err(ImpexError::UnsupportedFormat);
            }
            _ => {}
        }
    }
}

fn take_common(attributes: &mut Vec<OwnedAttribute>, offset: (i32, i32), id: LayerID) -> OraCommon {
    OraCommon {
        name: take_attribute(attributes, "name", None).unwrap_or_default(),
        offset: (
            offset.0
                + take_attribute(attributes, "x", None)
                    .and_then(|a| a.parse::<i32>().ok())
                    .unwrap_or(0),
            offset.1
                + take_attribute(attributes, "y", None)
                    .and_then(|a| a.parse::<i32>().ok())
                    .unwrap_or(0),
        ),
        opacity: take_attribute(attributes, "opacity", None)
            .and_then(|a| a.parse::<f32>().ok())
            .unwrap_or(1.0),
        visibility: take_attribute(attributes, "visibility", None).map_or(true, |a| a == "visible"),
        locked: take_attribute(attributes, "edit-locked", None).map_or(false, |a| a == "true"),
        censored: take_attribute(attributes, "censored", Some(DP_NAMESPACE))
            .map_or(false, |a| a == "true"),
        composite_op: take_attribute(attributes, "composite-op", None)
            .and_then(|s| Blendmode::from_svg_name(&s))
            .unwrap_or(Blendmode::Normal),
        unsupported_features: false,
        id,
    }
}

fn parse_stack_stack<R: Read>(
    mut attributes: Vec<OwnedAttribute>,
    next_layer_id: &mut LayerID,
    offset: (i32, i32),
    parser: &mut EventReader<R>,
) -> Result<OraStack, ImpexError> {
    let mut stack = OraStack {
        common: take_common(&mut attributes, offset, *next_layer_id),
        isolation: match &take_attribute(&mut attributes, "isolation", None).as_deref() {
            Some("isolate") => Isolation::Isolate,
            _ => Isolation::Auto,
        },
        layers: Vec::new(),
        annotations: Vec::new(),
    };

    *next_layer_id += 1;

    loop {
        match parser.next() {
            Ok(XmlEvent::StartElement {
                name, attributes, ..
            }) => {
                if name.local_name == "stack" {
                    let substack =
                        parse_stack_stack(attributes, next_layer_id, stack.common.offset, parser)?;
                    stack.common.unsupported_features |= substack.common.unsupported_features;

                    stack.layers.push(OraStackElement::Stack(substack));
                } else if name.local_name == "layer" {
                    let layer =
                        parse_stack_layer(attributes, *next_layer_id, stack.common.offset, parser)?;
                    *next_layer_id += 1;
                    stack.common.unsupported_features |= layer.common.unsupported_features;
                    stack.layers.push(OraStackElement::Layer(layer));
                } else if name.local_name == "annotations"
                    && name.namespace.as_deref() == Some(DP_NAMESPACE)
                {
                    // Backward compatibility.
                    // Drawpile versions <2.2 put the annotations element
                    // under the stack element.
                    stack.annotations.append(&mut parse_annotations(parser)?);
                } else {
                    warn!("Unsupported OpenRaster stack element <{}>", name.local_name);
                    stack.common.unsupported_features = true;
                    skip_element(parser)?;
                }
            }
            Ok(XmlEvent::EndElement { name, .. }) => {
                assert_eq!(name.local_name, "stack");
                return Ok(stack);
            }
            Ok(XmlEvent::EndDocument) => {
                warn!("Error reading OpenRaster stack: Unexpected end of document while parsing <image>");
                return Err(ImpexError::UnsupportedFormat);
            }
            Err(e) => {
                warn!("Error reading OpenRaster stack: {}", e);
                return Err(ImpexError::UnsupportedFormat);
            }
            _ => (),
        }
    }
}
fn parse_stack_layer<R: Read>(
    mut attributes: Vec<OwnedAttribute>,
    layer_id: LayerID,
    offset: (i32, i32),
    parser: &mut EventReader<R>,
) -> Result<OraLayer, ImpexError> {
    let mut layer = OraLayer {
        common: take_common(&mut attributes, offset, layer_id),
        filename: take_attribute(&mut attributes, "src", None)
            .ok_or(ImpexError::UnsupportedFormat)?,
        bgtile: take_attribute(&mut attributes, "background-tile", Some(MYPAINT_NAMESPACE))
            .unwrap_or_default(),
    };

    if !attributes.is_empty() {
        warn!("Unsupported OpenRaster layer attributes: {:?}", attributes);
        layer.common.unsupported_features = true;
    }

    // Layer element shouldn't have any child elements
    loop {
        match parser.next() {
            Ok(XmlEvent::StartElement { name, .. }) => {
                warn!("Unsupported OpenRaster layer element <{}>", name.local_name);
                layer.common.unsupported_features = true;
                skip_element(parser)?;
            }
            Ok(XmlEvent::EndElement { name, .. }) => {
                assert_eq!(name.local_name, "layer");
                return Ok(layer);
            }
            Ok(XmlEvent::EndDocument) => {
                warn!("Error reading OpenRaster stack: Unexpected end of document while parsing <image>");
                return Err(ImpexError::UnsupportedFormat);
            }
            Err(e) => {
                warn!("Error reading OpenRaster stack: {}", e);
                return Err(ImpexError::UnsupportedFormat);
            }
            _ => (),
        }
    }
}

fn parse_annotations<R: Read>(
    parser: &mut EventReader<R>,
) -> Result<Vec<OraAnnotation>, ImpexError> {
    let mut annotations = Vec::new();

    loop {
        match parser.next() {
            Ok(XmlEvent::StartElement {
                name, attributes, ..
            }) => {
                if name.local_name == "a" {
                    annotations.push(parse_annotation(attributes, parser)?);
                } else {
                    warn!(
                        "Unsupported OpenRaster annotations element <{}>",
                        name.local_name
                    );
                    skip_element(parser)?;
                }
            }
            Ok(XmlEvent::EndElement { name, .. }) => {
                assert_eq!(name.local_name, "annotations");
                return Ok(annotations);
            }
            Ok(XmlEvent::EndDocument) => {
                warn!("Error reading OpenRaster annotations: Unexpected end of document while parsing <image>");
                return Err(ImpexError::UnsupportedFormat);
            }
            Err(e) => {
                warn!("Error reading OpenRaster annotations: {}", e);
                return Err(ImpexError::UnsupportedFormat);
            }
            _ => (),
        }
    }
}

fn parse_annotation<R: Read>(
    mut attributes: Vec<OwnedAttribute>,
    parser: &mut EventReader<R>,
) -> Result<OraAnnotation, ImpexError> {
    let mut annotation = OraAnnotation {
        rect: Rectangle::new(
            take_attribute(&mut attributes, "x", None)
                .and_then(|a| a.parse::<i32>().ok())
                .unwrap_or(0),
            take_attribute(&mut attributes, "y", None)
                .and_then(|a| a.parse::<i32>().ok())
                .unwrap_or(0),
            take_attribute(&mut attributes, "w", None)
                .and_then(|a| a.parse::<i32>().ok())
                .unwrap_or(0),
            take_attribute(&mut attributes, "h", None)
                .and_then(|a| a.parse::<i32>().ok())
                .unwrap_or(0),
        ),
        bg: take_attribute(&mut attributes, "bg", None).unwrap_or_default(),
        valign: take_attribute(&mut attributes, "valign", None).unwrap_or_default(),
        content: String::new(),
    };

    loop {
        match parser.next() {
            Ok(XmlEvent::StartElement { name, .. }) => {
                warn!(
                    "Unsupported OpenRaster annotation element <{}>",
                    name.local_name
                );
                skip_element(parser)?;
            }
            Ok(XmlEvent::EndElement { name, .. }) => {
                assert_eq!(name.local_name, "a");
                return Ok(annotation);
            }
            Ok(XmlEvent::EndDocument) => {
                warn!("Error reading OpenRaster annotation: Unexpected end of document while parsing <image>");
                return Err(ImpexError::UnsupportedFormat);
            }
            Ok(XmlEvent::CData(s)) => annotation.content.push_str(&s),
            Ok(XmlEvent::Characters(s)) => annotation.content.push_str(&s),
            Err(e) => {
                warn!("Error reading OpenRaster annotation: {}", e);
                return Err(ImpexError::UnsupportedFormat);
            }
            _ => (),
        }
    }
}

fn parse_timeline<R: Read>(
    mut attributes: Vec<OwnedAttribute>,
    parser: &mut EventReader<R>,
) -> Result<OraTimeline, ImpexError> {
    let mut timeline = OraTimeline {
        frames: Vec::new(),
        enabled: take_attribute(&mut attributes, "enabled", None).map_or(true, |a| a == "true"),
    };

    loop {
        match parser.next() {
            Ok(XmlEvent::StartElement { name, .. }) => {
                if name.local_name == "frame" {
                    timeline.frames.push(parse_frame(parser)?);
                } else {
                    warn!("Unsupported timeline element <{}>", name.local_name);
                    skip_element(parser)?;
                }
            }
            Ok(XmlEvent::EndElement { name, .. }) => {
                assert_eq!(name.local_name, "timeline");
                return Ok(timeline);
            }
            Ok(XmlEvent::EndDocument) => {
                warn!(
                    "Error reading timeline: Unexpected end of document while parsing <timeline>"
                );
                return Err(ImpexError::UnsupportedFormat);
            }
            Err(e) => {
                warn!("Error reading timeline: {}", e);
                return Err(ImpexError::UnsupportedFormat);
            }
            _ => (),
        }
    }
}

fn parse_frame<R: Read>(parser: &mut EventReader<R>) -> Result<Vec<i32>, ImpexError> {
    let mut content = String::new();

    loop {
        match parser.next() {
            Ok(XmlEvent::StartElement { name, .. }) => {
                warn!("Unsupported frame element <{}>", name.local_name);
                skip_element(parser)?;
            }
            Ok(XmlEvent::EndElement { name, .. }) => {
                assert_eq!(name.local_name, "frame");

                return Ok(content.split(' ').filter_map(|s| s.parse().ok()).collect());
            }
            Ok(XmlEvent::EndDocument) => {
                warn!("Error reading timeline: Unexpected end of document while parsing <frame>");
                return Err(ImpexError::UnsupportedFormat);
            }
            Ok(XmlEvent::CData(s)) => content.push_str(&s),
            Ok(XmlEvent::Characters(s)) => content.push_str(&s),
            Err(e) => {
                warn!("Error reading frame: {}", e);
                return Err(ImpexError::UnsupportedFormat);
            }
            _ => (),
        }
    }
}

fn skip_element<R: Read>(parser: &mut EventReader<R>) -> Result<(), ImpexError> {
    let mut depth = 1;
    loop {
        match parser.next() {
            Ok(XmlEvent::StartElement { .. }) => {
                depth += 1;
            }
            Ok(XmlEvent::EndElement { .. }) => {
                depth -= 1;
                if depth == 0 {
                    return Ok(());
                }
            }
            Ok(XmlEvent::EndDocument) => {
                warn!("Error reading OpenRaster stack: Unexpected end of document");
                return Err(ImpexError::UnsupportedFormat);
            }
            Err(e) => {
                warn!("Error reading OpenRaster stack: {}", e);
                return Err(ImpexError::UnsupportedFormat);
            }
            _ => (),
        }
    }
}

fn take_attribute(
    attrs: &mut Vec<OwnedAttribute>,
    name: &str,
    namespace: Option<&str>,
) -> Option<String> {
    attrs
        .iter()
        .position(|a| a.name.local_name == name && a.name.namespace.as_deref() == namespace)
        .map(|idx| attrs.remove(idx).value)
}

fn make_idlist(stack: &OraStack) -> Vec<LayerID> {
    let mut ids = Vec::new();
    for layer in stack.layers.iter() {
        match layer {
            OraStackElement::Stack(s) => {
                ids.push(s.common.id);
                ids.append(&mut make_idlist(s));
            }
            OraStackElement::Layer(l) => {
                ids.push(l.common.id);
            }
        }
    }
    ids
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_load_openraster() {
        let path = concat!(env!("CARGO_MANIFEST_DIR"), "/testdata/oratest.ora");
        let ls = load_openraster_image(path.as_ref()).unwrap();
        let annotations = ls.get_annotations();

        assert_eq!(ls.root().layer_count(), 3);
        assert_eq!(annotations.len(), 1);

        assert!(matches!(annotations[0].valign, VAlign::Center));
    }
}
