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

use super::conv::to_dpimage;
use super::ora_utils::{DP_NAMESPACE, MYPAINT_NAMESPACE};
use super::{ImageImportResult, ImpexError};
use dpcore::paint::annotation::VAlign;
use dpcore::paint::layerstack::{LayerFill, LayerInsertion};
use dpcore::paint::{editlayer, Blendmode, Color, Image, LayerStack, Rectangle, Size, Tile};

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

    // TODO dpi

    // Create layers
    let mut layer_id = 0x0101;
    for oralayer in canvas.layers {
        if !oralayer.bgtile.is_empty() {
            let tile_image = get_image_file(&mut archive, &oralayer.bgtile)?;
            if tile_image.width == 64 || tile_image.height == 64 {
                // If we have a valid tile, this is a background layer
                // that we don't need to represent as an actual layer
                ls.background = Tile::from_data(&tile_image.pixels, 0);
                continue;
            }
        }

        let img = get_image_file(&mut archive, &oralayer.filename)?;

        let mut layer = ls
            .add_layer(
                layer_id,
                LayerFill::Solid(Color::TRANSPARENT),
                LayerInsertion::Top,
            )
            .unwrap();

        layer.title = format!("Layer {}", layer_id - 0x0100);
        layer.opacity = oralayer.opacity.clamp(0.0, 1.0);
        layer.hidden = !oralayer.visibility;
        layer.censored = oralayer.censored;
        layer.fixed = oralayer.fixed;
        layer.blendmode = Blendmode::from_svg_name(&oralayer.composite_op).unwrap_or_default();

        // TODO layer lock status

        editlayer::draw_image(
            &mut layer,
            1,
            &img.pixels,
            &Rectangle::new(
                oralayer.offset.0,
                oralayer.offset.1,
                img.width as i32,
                img.height as i32,
            ),
            1.0,
            Blendmode::Replace,
        );

        layer_id += 1;
    }

    // Create annotations
    let annotation_id = 0x0101;
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
    }

    // TODO warn about unsupported features
    Ok(ls)
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
) -> Result<Image, ImpexError> {
    let mut filecontent = Vec::new();
    archive.by_name(filename)?.read_to_end(&mut filecontent)?;
    let img = ImageReader::new(Cursor::new(filecontent))
        .with_guessed_format()?
        .decode()?;

    Ok(to_dpimage(&img.into_rgba8()))
}

#[derive(Debug)]
struct OraCanvas {
    size: Size,
    unsupported_features: bool,
    dpi: (i32, i32),
    layers: Vec<OraLayer>,           // note: nested stacks not yet supported
    annotations: Vec<OraAnnotation>, // our extension
}

#[derive(Debug)]
struct OraLayer {
    name: String,
    filename: String,
    bgtile: String, // MyPaint extension
    offset: (i32, i32),
    opacity: f32,
    visibility: bool,
    locked: bool,
    censored: bool, // our extension
    fixed: bool,    // our extension
    composite_op: String,
    unsupported_features: bool, // this layer makes use of unsupported features
}

#[derive(Debug)]
struct OraAnnotation {
    rect: Rectangle,
    bg: String,
    valign: String,
    content: String,
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
        layers: Vec::new(),
        annotations: Vec::new(),
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
                    parse_stack_stack(&mut canvas, attributes, (0, 0), parser)?;
                } else {
                    warn!("Unsupported openraster <image> element <{}>", name);
                    canvas.unsupported_features = true;
                    skip_element(parser)?;
                }
            }
            Ok(XmlEvent::EndElement { name, .. }) => {
                assert_eq!(name.local_name, "image");
                // Note: layers are listed top-to-bottom in the XML,
                // but are stored bottom-to-top inside Drawpile
                canvas.layers.reverse();
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

fn parse_stack_stack<R: Read>(
    canvas: &mut OraCanvas,
    mut attributes: Vec<OwnedAttribute>,
    offset: (i32, i32),
    parser: &mut EventReader<R>,
) -> Result<(), ImpexError> {
    let offset = (
        offset.0
            + take_attribute(&mut attributes, "x", None)
                .and_then(|a| a.parse::<i32>().ok())
                .unwrap_or(0),
        offset.1
            + take_attribute(&mut attributes, "y", None)
                .and_then(|a| a.parse::<i32>().ok())
                .unwrap_or(0),
    );

    loop {
        match parser.next() {
            Ok(XmlEvent::StartElement {
                name, attributes, ..
            }) => {
                if name.local_name == "stack" {
                    // Nested stacks aren't supported at the moment, but we can
                    // still get the layers out of  them.
                    canvas.unsupported_features = true;
                    parse_stack_stack(canvas, attributes, offset, parser)?;
                } else if name.local_name == "layer" {
                    canvas
                        .layers
                        .push(parse_stack_layer(attributes, offset, parser)?);
                } else if name.local_name == "annotations"
                    && name.namespace.as_deref() == Some(DP_NAMESPACE)
                {
                    canvas.annotations.append(&mut parse_annotations(parser)?);
                } else {
                    warn!("Unsupported OpenRaster stack element <{}>", name.local_name);
                    canvas.unsupported_features = true;
                    skip_element(parser)?;
                }
            }
            Ok(XmlEvent::EndElement { name, .. }) => {
                assert_eq!(name.local_name, "stack");
                return Ok(());
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
    offset: (i32, i32),
    parser: &mut EventReader<R>,
) -> Result<OraLayer, ImpexError> {
    let mut layer = OraLayer {
        name: take_attribute(&mut attributes, "name", None).unwrap_or(String::new()),
        filename: take_attribute(&mut attributes, "src", None)
            .ok_or(ImpexError::UnsupportedFormat)?,
        bgtile: take_attribute(&mut attributes, "background-tile", Some(MYPAINT_NAMESPACE))
            .unwrap_or(String::new()),
        offset: (
            offset.0
                + take_attribute(&mut attributes, "x", None)
                    .and_then(|a| a.parse::<i32>().ok())
                    .unwrap_or(0),
            offset.1
                + take_attribute(&mut attributes, "y", None)
                    .and_then(|a| a.parse::<i32>().ok())
                    .unwrap_or(0),
        ),
        opacity: take_attribute(&mut attributes, "opacity", None)
            .and_then(|a| a.parse::<f32>().ok())
            .unwrap_or(1.0),
        visibility: take_attribute(&mut attributes, "visibility", None)
            .map_or(true, |a| a == "visibile"),
        locked: take_attribute(&mut attributes, "edit-locked", None).map_or(false, |a| a == "true"),
        censored: take_attribute(&mut attributes, "censored", Some(DP_NAMESPACE))
            .map_or(false, |a| a == "true"),
        fixed: take_attribute(&mut attributes, "fixed", Some(DP_NAMESPACE))
            .map_or(false, |a| a == "true"),
        composite_op: take_attribute(&mut attributes, "composite-op", None)
            .unwrap_or("src-over".into()),
        unsupported_features: false,
    };

    if !attributes.is_empty() {
        warn!("Unsupported OpenRaster layer attributes: {:?}", attributes);
        layer.unsupported_features = true;
    }

    // Layer element shouldn't have any child elements
    loop {
        match parser.next() {
            Ok(XmlEvent::StartElement { name, .. }) => {
                warn!("Unsupported OpenRaster layer element <{}>", name.local_name);
                layer.unsupported_features = true;
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
        bg: take_attribute(&mut attributes, "bg", None).unwrap_or(String::new()),
        valign: take_attribute(&mut attributes, "valign", None).unwrap_or(String::new()),
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
    if let Some(idx) = attrs
        .iter()
        .position(|a| a.name.local_name == name && a.name.namespace.as_deref() == namespace)
    {
        Some(attrs.remove(idx).value)
    } else {
        None
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::path::PathBuf;

    #[test]
    fn test_load_openraster() {
        let path: PathBuf = [env!("CARGO_MANIFEST_DIR"), "testdata", "oratest.ora"]
            .iter()
            .collect();
        let ls = load_openraster_image(path.as_ref()).unwrap();
        let annotations = ls.get_annotations();

        assert_eq!(ls.layer_count(), 3);
        assert_eq!(annotations.len(), 1);

        assert!(matches!(annotations[0].valign, VAlign::Center));
    }
}
