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

use super::brushstate::BrushState;
use super::classicbrush::{ClassicBrush, ClassicBrushShape};
use crate::paint::{BitmapLayer, Blendmode, Color};
use crate::protocol::message::{CommandMessage, DrawDabsPixelMessage, PixelDab};
use crate::protocol::MessageWriter;

use std::mem;

pub struct PixelBrushState {
    brush: ClassicBrush,
    layer_id: u16,
    length: i32,
    smudge_distance: i32,
    smudge_color: Color,
    in_progress: bool,
    last_x: f32,
    last_y: f32,
    last_p: f32,

    dabs: Vec<DrawDabsPixelMessage>,
    last_dab_x: i32,
    last_dab_y: i32,
}

impl PixelBrushState {
    pub fn new() -> PixelBrushState {
        PixelBrushState {
            brush: ClassicBrush::new(),
            layer_id: 0,
            length: 0,
            smudge_distance: 0,
            smudge_color: Color::TRANSPARENT,
            in_progress: false,
            last_x: 0.0,
            last_y: 0.0,
            last_p: 0.0,
            dabs: Vec::new(),
            last_dab_x: 0,
            last_dab_y: 0,
        }
    }

    pub fn set_brush(&mut self, brush: ClassicBrush) {
        self.brush = brush;

        let mut color = self.brush.color;
        if self.brush.incremental || self.brush.smudge.1 > 0.0 {
            // Incremental mode must be used when smudging, because
            // color is not picked up from sublayers
            color.a = 0.0;
        } else {
            // If brush color alpha is nonzero, indirect drawing mode
            // is used and the alpha is used as the overall transparency
            // of the entire stroke.
            // TODO this doesn't work right. We should use alpha-darken mode
            // and set the opacity range properly
            color.a = self.brush.opacity.1;
            self.brush.opacity.1 = 1.0;
            if self.brush.opacity_pressure {
                self.brush.opacity.0 = 0.0;
            }
        }

        self.brush.color = color;
        self.smudge_color = color;
    }

    fn add_dab(&mut self, x: i32, y: i32, p: f32, source: Option<&BitmapLayer>) {
        let smudge = self.brush.smudge_at(p);
        let dia = self.brush.size_at(p).clamp(1.0, 255.0) as u8;

        self.smudge_distance += 1;
        if smudge > 0.0 && self.smudge_distance > self.brush.resmudge {
            if let Some(sl) = source {
                let sample = sl.sample_color(x, y, dia as i32);
                if sample.a > 0.0 {
                    let a = sample.a * smudge;
                    self.smudge_color.r = self.smudge_color.r * (1.0 - a) + sample.r * a;
                    self.smudge_color.g = self.smudge_color.g * (1.0 - a) + sample.g * a;
                    self.smudge_color.b = self.smudge_color.b * (1.0 - a) + sample.b * a;
                }
            }
            self.smudge_distance = 0;
        }

        let opacity = (self.brush.opacity_at(p) * 255.0) as u8;
        let color = self.smudge_color.as_argb32();

        const MAX_XY_DELTA: i32 = 127;
        let dx = x - self.last_dab_x;
        let dy = y - self.last_dab_y;

        if self.dabs.is_empty()
            || dx.abs() > MAX_XY_DELTA
            || dy.abs() > MAX_XY_DELTA
            || self.dabs.last().unwrap().dabs.len() >= DrawDabsPixelMessage::MAX_PIXELDABS
            || self.dabs.last().unwrap().color != color
        {
            self.dabs.push(DrawDabsPixelMessage {
                layer: self.layer_id,
                x,
                y,
                color,
                mode: self.brush.mode.into(),
                dabs: vec![PixelDab {
                    x: 0,
                    y: 0,
                    size: dia,
                    opacity,
                }],
            });
        } else {
            self.dabs.last_mut().unwrap().dabs.push(PixelDab {
                x: dx as i8,
                y: dy as i8,
                size: dia,
                opacity,
            });
        }

        self.last_dab_x = x;
        self.last_dab_y = y;
    }
}

impl BrushState for PixelBrushState {
    fn set_layer(&mut self, layer_id: u16) {
        self.layer_id = layer_id;
    }

    fn stroke_to(
        &mut self,
        x: f32,
        y: f32,
        p: f32,
        _delta_msec: i64,
        source: Option<&BitmapLayer>,
    ) {
        if self.in_progress {
            let dp = (p - self.last_p) / (x - self.last_x).hypot(y - self.last_y);

            let mut x0 = self.last_x as i32;
            let mut y0 = self.last_y as i32;
            let p = self.last_p;
            let x1 = x as i32;
            let y1 = y as i32;

            let dy = y1 - y0;
            let dx = x1 - x0;

            let (stepy, dy) = if dy < 0 { (-1, dy * -2) } else { (1, dy * 2) };

            let (stepx, dx) = if dx < 0 { (-1, dx * -2) } else { (1, dx * 2) };

            let mut distance = self.length;
            let mut pressure = p;

            if dx > dy {
                let mut fraction = dy - (dx / 2);
                while x0 != x1 {
                    let spacing = self.brush.spacing_at(pressure) as i32;
                    if fraction >= 0 {
                        y0 += stepy;
                        fraction -= dx;
                    }
                    x0 += stepx;
                    fraction += dy;
                    distance += 1;
                    if distance >= spacing {
                        self.add_dab(x0, y0, pressure, source);
                        distance = 0;
                    }
                    pressure += dp;
                }
            } else {
                let mut fraction = dx - (dy / 2);
                while y0 != y1 {
                    let spacing = self.brush.spacing_at(pressure) as i32;
                    if fraction >= 0 {
                        x0 += stepx;
                        fraction -= dy;
                    }
                    y0 += stepy;
                    fraction += dx;
                    distance += 1;
                    if distance >= spacing {
                        self.add_dab(x0, y0, pressure, source);
                        distance = 0;
                    }
                    pressure += dp;
                }
            }

            self.length = distance;
        } else {
            // Start of a new stroke
            self.in_progress = true;
            if self.brush.colorpick && self.brush.mode != Blendmode::Erase {
                if let Some(sl) = source {
                    self.smudge_color = sl.sample_color(
                        x as i32,
                        y as i32,
                        self.brush.size_at(p).clamp(1.0, 128.0) as i32,
                    );
                    self.smudge_distance = -1;
                }
            }
            self.add_dab(x as i32, y as i32, p, source);
        }

        self.last_x = x;
        self.last_y = y;
        self.last_p = p;
    }

    fn end_stroke(&mut self) {
        self.in_progress = false;
        self.length = 0;
        self.smudge_distance = 0;
        self.smudge_color = self.brush.color;
    }

    fn take_dabs(&mut self, user_id: u8) -> Vec<CommandMessage> {
        let dabs = mem::take(&mut self.dabs);
        match self.brush.shape {
            ClassicBrushShape::RoundPixel => dabs
                .into_iter()
                .map(|d| CommandMessage::DrawDabsPixel(user_id, d))
                .collect(),
            _ => dabs
                .into_iter()
                .map(|d| CommandMessage::DrawDabsPixelSquare(user_id, d))
                .collect(),
        }
    }

    fn write_dabs(&mut self, user_id: u8, writer: &mut MessageWriter) {
        let dabs = mem::take(&mut self.dabs);
        match self.brush.shape {
            ClassicBrushShape::RoundPixel => {
                for d in dabs {
                    CommandMessage::DrawDabsPixel(user_id, d).write(writer);
                }
            }
            _ => {
                for d in dabs {
                    CommandMessage::DrawDabsPixelSquare(user_id, d).write(writer);
                }
            }
        };
    }

    fn add_offset(&mut self, x: f32, y: f32) {
        self.last_x += x;
        self.last_y += y;
    }
}

impl Default for PixelBrushState {
    fn default() -> Self {
        Self::new()
    }
}
