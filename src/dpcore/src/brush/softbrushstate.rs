// This file is part of Drawpile.
// Copyright (C) 2020-2022 Calle Laakkonen
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
use super::classicbrush::ClassicBrush;
use crate::paint::{BitmapLayer, Blendmode, Color};
use crate::protocol::message::{ClassicDab, CommandMessage, DrawDabsClassicMessage};
use crate::protocol::MessageWriter;

use std::mem;

pub struct SoftBrushState {
    brush: ClassicBrush,
    layer_id: u16,
    length: f32,
    smudge_distance: i32,
    smudge_color: Color,
    in_progress: bool,
    last_x: f32,
    last_y: f32,
    last_p: f32,

    dabs: Vec<DrawDabsClassicMessage>,
    last_dab_x: i32,
    last_dab_y: i32,
}

impl SoftBrushState {
    pub fn new() -> SoftBrushState {
        SoftBrushState {
            brush: ClassicBrush::new(),
            layer_id: 0,
            length: 0.0,
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

    fn add_dab(&mut self, x: f32, y: f32, p: f32) {
        let opacity = (self.brush.opacity_at(p) * 255.0) as u8;
        if opacity == 0 {
            return;
        }
        let hardness = (self.brush.hardness_at(p) * 255.0) as u8;
        let size = (self.brush.size_at(p) * 256.0) as u16;
        let color = self.smudge_color.as_argb32();
        let ix = (x * 4.0) as i32;
        let iy = (y * 4.0) as i32;

        const MAX_XY_DELTA: i32 = 127;
        let dx = ix - self.last_dab_x;
        let dy = iy - self.last_dab_y;

        if self.dabs.is_empty()
            || dx.abs() > MAX_XY_DELTA
            || dy.abs() > MAX_XY_DELTA
            || self.dabs.last().unwrap().dabs.len() >= DrawDabsClassicMessage::MAX_CLASSICDABS
            || self.dabs.last().unwrap().color != color
        {
            self.dabs.push(DrawDabsClassicMessage {
                layer: self.layer_id,
                x: ix,
                y: iy,
                color,
                mode: self.brush.mode.into(),
                dabs: vec![ClassicDab {
                    x: 0,
                    y: 0,
                    size,
                    hardness,
                    opacity,
                }],
            });
        } else {
            self.dabs.last_mut().unwrap().dabs.push(ClassicDab {
                x: dx as i8,
                y: dy as i8,
                size,
                hardness,
                opacity,
            });
        }

        self.last_dab_x = ix;
        self.last_dab_y = iy;
    }
}

impl BrushState for SoftBrushState {
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
            let dx = x - self.last_x;
            let dy = y - self.last_y;
            let dist = dx.hypot(dy);
            if dist < 0.25 {
                return;
            }

            let dx = dx / dist;
            let dy = dy / dist;
            let dp = (p - self.last_p) / dist;

            let spacing0 = self.brush.spacing_at(self.last_p).max(1.0);

            let mut i = if self.length > spacing0 {
                0.0
            } else if self.length == 0.0 {
                spacing0
            } else {
                self.length
            };

            let mut dab_x = self.last_x + dx * i;
            let mut dab_y = self.last_y + dy * i;
            let mut dab_p = (self.last_p + dp * i).clamp(0.0, 1.0);

            while i <= dist {
                let spacing = self.brush.spacing_at(dab_p).max(1.0);
                let smudge = self.brush.smudge_at(dab_p);

                self.smudge_distance += 1;
                if smudge > 0.0 && self.smudge_distance > self.brush.resmudge {
                    if let Some(sl) = source {
                        let sample = sl.sample_color(
                            dab_x as i32,
                            dab_y as i32,
                            self.brush.size_at(dab_p).clamp(1.0, 128.0) as i32,
                        );
                        if sample.a > 0.0 {
                            let a = sample.a * smudge;
                            self.smudge_color.r = self.smudge_color.r * (1.0 - a) + sample.r * a;
                            self.smudge_color.g = self.smudge_color.g * (1.0 - a) + sample.g * a;
                            self.smudge_color.b = self.smudge_color.b * (1.0 - a) + sample.b * a;
                        }
                    }
                    self.smudge_distance = 0;
                }

                self.add_dab(dab_x, dab_y, dab_p);

                dab_x += dx * spacing;
                dab_y += dy * spacing;
                dab_p = (dab_p + dp * spacing).clamp(0.0, 1.0);
                i += spacing;
            }
            self.length = i - dist;
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

            self.add_dab(x, y, p);
        }

        self.last_x = x;
        self.last_y = y;
        self.last_p = p;
    }

    fn end_stroke(&mut self) {
        self.in_progress = false;
        self.length = 0.0;
        self.smudge_distance = 0;
        self.smudge_color = self.brush.color;
    }

    fn take_dabs(&mut self, user_id: u8) -> Vec<CommandMessage> {
        let dabs = mem::take(&mut self.dabs);
        dabs.into_iter()
            .map(|d| CommandMessage::DrawDabsClassic(user_id, d))
            .collect()
    }

    fn write_dabs(&mut self, user_id: u8, writer: &mut MessageWriter) {
        // TODO excessive copying. We should be able to preserve
        // the vector and not make temporary copies of the individual
        // messages.
        let dabs = mem::take(&mut self.dabs);
        for d in dabs {
            CommandMessage::DrawDabsClassic(user_id, d).write(writer);
        }
    }

    fn add_offset(&mut self, x: f32, y: f32) {
        self.last_x += x;
        self.last_y += y;
    }
}
