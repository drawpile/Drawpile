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

use dpcore::brush::{BrushEngine, BrushState, ClassicBrush};
use dpcore::canvas::brushes;
use dpcore::paint::layerstack::{LayerFill, LayerInsertion, LayerStack};
use dpcore::paint::tile::Tile;
use dpcore::paint::{
    editlayer, floodfill, Blendmode, BrushMask, ClassicBrushCache, Color, LayerID, Rectangle,
};
use dpcore::protocol::message::CommandMessage;

#[repr(C)]
pub enum BrushPreviewShape {
    Stroke,
    Line,
    Rectangle,
    Ellipse,
    FloodFill,
    FloodErase,
}

enum ForegroundStyle {
    Solid,
    RainbowBars,
    RainbowDabs,
}

pub struct BrushPreview {
    pub layerstack: LayerStack,
}

const LAYER_ID: LayerID = 1;

impl BrushPreview {
    pub fn new(width: u32, height: u32) -> BrushPreview {
        let mut bp = BrushPreview {
            layerstack: LayerStack::new(width, height),
        };

        bp.layerstack.add_layer(
            LAYER_ID,
            LayerFill::Solid(Color::TRANSPARENT),
            LayerInsertion::Top,
        );
        bp
    }

    pub fn render(&mut self, brush: &ClassicBrush, shape: BrushPreviewShape) {
        let mut brush = brush.clone();

        // Prepare background and foreground colors
        let mut background_color = if brush.color.is_dark() {
            Color::rgb8(250, 250, 250)
        } else {
            Color::rgb8(32, 32, 32)
        };

        let mut layer_color = Color::TRANSPARENT;
        let mut foreground_style = if brush.smudge.1 > 0.0 {
            ForegroundStyle::RainbowBars
        } else {
            ForegroundStyle::Solid
        };

        match brush.mode {
            Blendmode::Erase => {
                layer_color = background_color;
                background_color = Color::TRANSPARENT;
            }
            Blendmode::ColorErase => {
                background_color = Color::TRANSPARENT;
                layer_color = brush.color;
            }
            m => {
                if !m.can_increase_opacity() {
                    foreground_style = ForegroundStyle::RainbowDabs;
                    background_color = Color::TRANSPARENT;
                }
            }
        }

        match shape {
            BrushPreviewShape::FloodFill => {
                brush.color = background_color;
                background_color = Color::TRANSPARENT;
            }
            BrushPreviewShape::FloodErase => {
                layer_color = brush.color;
                brush.color = background_color;
                background_color = Color::TRANSPARENT;
            }
            _ => {}
        }

        // Draw the background
        self.layerstack.background = Tile::new(&background_color, 1);
        let layer = self.layerstack.get_layer_mut(LAYER_ID).unwrap();
        editlayer::put_tile(layer, 0, 0, 0, 9999, &Tile::new(&layer_color, 1));

        match foreground_style {
            ForegroundStyle::Solid => {
                // When using the "behind" mode, draw something in the foreground to show off the effect
                if brush.mode == Blendmode::Behind {
                    let w = layer.width() as i32;
                    let h = layer.height() as i32;
                    let b = (w / 20).max(2) as i32;
                    editlayer::fill_rect(
                        layer,
                        1,
                        &Color::BLACK,
                        Blendmode::Normal,
                        &Rectangle::new((w / 4 - b / 2) as i32, 0, b, h),
                    );
                    editlayer::fill_rect(
                        layer,
                        1,
                        &Color::BLACK,
                        Blendmode::Normal,
                        &Rectangle::new((w / 4 * 2 - b / 2) as i32, 0, b, h),
                    );
                    editlayer::fill_rect(
                        layer,
                        1,
                        &Color::BLACK,
                        Blendmode::Normal,
                        &Rectangle::new((w / 4 * 3 - b / 2) as i32, 0, b, h),
                    );
                }
            }
            ForegroundStyle::RainbowDabs => {
                let w = layer.width() as i32;
                let h = layer.height() as i32;
                let d = (h * 2 / 3).max(10).min(255);
                let x0 = d;
                let x1 = w - d;
                let step = d * 70 / 100;
                let huestep = 359.0 / ((x1 - x0) as f32 / step as f32);
                let mut hue = 0.0;
                let dabmask = BrushMask::new_round_pixel(d as u32, 1.0);
                for x in (x0..x1).step_by(step as usize) {
                    let color = Color::from_hsv(hue, 0.62, 0.86);
                    editlayer::draw_brush_dab(
                        layer,
                        1,
                        x - d / 2,
                        h / 2 - d / 2,
                        &dabmask,
                        &color,
                        Blendmode::Normal,
                    );
                    hue += huestep;
                }
            }
            ForegroundStyle::RainbowBars => {
                let w = layer.width() as i32;
                let h = layer.height() as i32;
                let bars = 5;
                let bw = w / bars;
                for i in 0..bars {
                    let color = Color::from_hsv(i as f32 * 360.0 / bars as f32, 0.62, 0.86);
                    editlayer::fill_rect(
                        layer,
                        1,
                        &color,
                        Blendmode::Normal,
                        &Rectangle::new(i * bw, 0, bw, h),
                    );
                }
            }
        }

        // Draw the preview brush stroke
        let preview_rect = Rectangle::new(
            layer.width() as i32 / 8,
            layer.height() as i32 / 4,
            (layer.width() - layer.width() / 4) as i32,
            (layer.height() - layer.height() / 2) as i32,
        );

        let points = match shape {
            BrushPreviewShape::Stroke => make_strokeshape(preview_rect),
            BrushPreviewShape::Line => vec![
                (preview_rect.x as f32, preview_rect.y as f32, 1.0),
                (
                    preview_rect.right() as f32,
                    preview_rect.bottom() as f32,
                    1.0,
                ),
            ],
            BrushPreviewShape::Rectangle => make_rect_shape(preview_rect),
            BrushPreviewShape::Ellipse => make_ellipse_shape(preview_rect),
            BrushPreviewShape::FloodFill | BrushPreviewShape::FloodErase => {
                make_blob_shape(preview_rect)
            }
        };

        let mut painter = BrushEngine::new();
        painter.set_classicbrush(brush);
        for p in points {
            painter.stroke_to(p.0, p.1, p.2, Some(layer));
        }
        painter.end_stroke();

        let mut brushcache = ClassicBrushCache::new();

        let dabs = painter.take_dabs(1);
        for dab in dabs {
            match dab {
                CommandMessage::DrawDabsClassic(_, m) => {
                    brushes::drawdabs_classic(layer, 1, &m, &mut brushcache)
                }
                CommandMessage::DrawDabsPixel(_, m) => brushes::drawdabs_pixel(layer, 1, &m, false),
                CommandMessage::DrawDabsPixelSquare(_, m) => {
                    brushes::drawdabs_pixel(layer, 1, &m, true)
                }
                _ => unimplemented!(),
            };
        }

        editlayer::merge_sublayer(layer, 1);
    }

    pub fn floodfill(&mut self, color: Color, tolerance: f32, expansion: i32, fill_under: bool) {
        let mut result = floodfill::floodfill(
            &self.layerstack,
            self.layerstack.width() as i32 / 2,
            self.layerstack.height() as i32 / 2,
            color,
            tolerance,
            LAYER_ID,
            false,
            1000 * 1000,
        );
        if result.image.is_null() {
            return;
        }

        if expansion > 0 {
            result = floodfill::expand_floodfill(result, expansion);
        }

        editlayer::draw_image(
            self.layerstack.get_layer_mut(LAYER_ID).unwrap(),
            1,
            &result.image.pixels,
            &Rectangle::new(
                result.x as i32,
                result.y as i32,
                result.image.width as i32,
                result.image.height as i32,
            ),
            1.0,
            if color.is_transparent() {
                Blendmode::Erase
            } else if fill_under {
                Blendmode::Behind
            } else {
                Blendmode::Normal
            },
        );
    }
}

type Point = (f32, f32, f32);

fn make_strokeshape(rect: Rectangle) -> Vec<Point> {
    let w = rect.w as f32;
    let h = rect.h as f32 * 0.6;
    let offy = rect.y as f32 + rect.h as f32 / 2.0;
    let dphase = (2.0 * std::f32::consts::PI) / w;
    let mut phase = 0.0f32;

    let mut points = Vec::new();
    points.reserve(w as usize);
    for x in 0..rect.w {
        let fx = x as f32 / w;
        let pressure = ((fx * fx - fx * fx * fx) * 6.756).min(1.0).max(0.0);
        let y = phase.sin() * h;
        points.push(((rect.x + x) as f32, offy + y, pressure));
        phase += dphase;
    }
    points
}

fn make_rect_shape(rect: Rectangle) -> Vec<Point> {
    let x0 = rect.x as f32;
    let x1 = rect.right() as f32;
    let y0 = rect.y as f32;
    let y1 = rect.bottom() as f32;
    vec![
        (x0, y0, 1.0),
        (x1, y0, 1.0),
        (x1, y1, 1.0),
        (x0, y1, 1.0),
        (x0, y0, 1.0),
    ]
}

fn make_ellipse_shape(rect: Rectangle) -> Vec<Point> {
    let a = rect.w as f32 / 2.0;
    let b = rect.h as f32 / 2.0;
    let cx = rect.x as f32 + a;
    let cy = rect.y as f32 + b;

    let mut points = Vec::new();

    let mut t = 0.0;
    while t < 2.0 * std::f32::consts::PI {
        points.push((cx + a * t.cos(), cy + b * t.sin(), 1.0));
        t += std::f32::consts::PI / 20.0;
    }
    points
}

// This is used to demonstrate flood fill
fn make_blob_shape(rect: Rectangle) -> Vec<Point> {
    let mid = (rect.y + rect.h / 2) as f32;
    let h = rect.h as f32 * 0.8;

    let mut points = Vec::new();
    let mut a = 0.0f32;
    while a < std::f32::consts::PI {
        let x = rect.x as f32 + (a / std::f32::consts::PI * rect.w as f32);
        let y = a.sin().powf(0.5) * 0.7 + (a * 3.0).sin() * 0.3;
        points.push((x, mid - y * h, 1.0));
        a += 0.1;
    }
    points.push((rect.right() as f32, mid, 1.0));

    a = 0.1;
    while a < std::f32::consts::PI {
        let x = rect.right() as f32 - (a / std::f32::consts::PI * rect.w as f32);
        let y = a.sin().powf(0.5) * 0.7 + (a * 2.8).sin() * 0.2;
        points.push((x, mid + y * h, 1.0));
        a += 0.1;
    }
    points.push(points[0]);

    points
}
