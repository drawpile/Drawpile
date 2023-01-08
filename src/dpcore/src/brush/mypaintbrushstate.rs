// This file is part of Drawpile.
// Copyright (C) 2022 askmeaboutloom
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
use super::mypaintbrush::{MyPaintBrush, MyPaintSettings};
use super::mypaintsurface::{MyPaintSurface, MyPaintTarget};
use crate::paint::{BitmapLayer, Color};
use crate::protocol::message::{CommandMessage, DrawDabsMyPaintMessage, MyPaintDab};
use crate::protocol::MessageWriter;
use dpmypaint as c;
use std::mem;

// This is the same number of smudge buckets that MyPaint uses.
const NUM_BUCKETS: i32 = 256;

static MYPAINT_INIT: std::sync::Once = std::sync::Once::new();

unsafe fn new_mypaint_brush() -> *mut c::MyPaintBrush {
    MYPAINT_INIT.call_once(|| c::mypaint_init());
    let brush = c::mypaint_brush_new_with_buckets(NUM_BUCKETS);
    c::mypaint_brush_from_defaults(brush);
    brush
}

// Reduce an arbitrary angle in degrees to a value between 0 and 255.
// That puts the angle resolution at around 1.4 degrees, which is good enough.
fn angle_to_u8(angle: f32) -> u8 {
    let a = (angle as i32) % 360;
    let b = if a < 0 { a + 360 } else { a };
    ((b as f32) / 360.0 * 255.0 + 0.5) as u8
}

// Spread the aspect ratio into a range between 0.1 and 10.0, then into a byte.
fn aspect_ratio_to_u8(aspect_ratio: f32) -> u8 {
    ((aspect_ratio.clamp(0.1, 10.0) - 0.1) * 25.755 + 0.5) as u8
}

struct MyPaintBrushStateTarget {
    layer_id: u16,
    lock_alpha: bool,
    erase: bool,
    dabs: Vec<DrawDabsMyPaintMessage>,
    last_dab_x: i32,
    last_dab_y: i32,
}

impl MyPaintBrushStateTarget {
    fn needs_new_dab(&mut self, dx: i32, dy: i32, color: u32, lock_alpha: u8) -> bool {
        if !self.dabs.is_empty() {
            let last = &self.dabs.last().unwrap();
            const MAX_XY_DELTA: i32 = 127;
            last.dabs.len() >= DrawDabsMyPaintMessage::MAX_MYPAINTDABS
                || dx.abs() > MAX_XY_DELTA
                || dy.abs() > MAX_XY_DELTA
                || last.color != color
                || last.lock_alpha != lock_alpha
        } else {
            true
        }
    }
}

impl MyPaintTarget for MyPaintBrushStateTarget {
    fn add_dab(
        &mut self,
        x: f32,
        y: f32,
        radius: f32,
        hardness: f32,
        opacity: f32,
        r: f32,
        g: f32,
        b: f32,
        a: f32,
        aspect_ratio: f32,
        angle: f32,
        lock_alpha: f32,
        // Colorize isn't supported while we still only have 8 bits to work with.
        _colorize: f32,
    ) {
        let color = if self.erase {
            Color::TRANSPARENT
        } else {
            Color { r, g, b, a }
        };

        let mut dab = MyPaintDab {
            x: 0i8,
            y: 0i8,
            size: (radius * 512.0 + 0.5).clamp(0.0, 65535.0) as u16,
            hardness: (hardness * 255.0 + 0.5).clamp(0.0, 255.0) as u8,
            opacity: (opacity * 255.0 + 0.5).clamp(0.0, 255.0) as u8,
            angle: angle_to_u8(angle),
            aspect_ratio: aspect_ratio_to_u8(aspect_ratio),
        };

        let dab_x = (x * 4.0) as i32;
        let dab_y = (y * 4.0) as i32;
        let dx = dab_x - self.last_dab_x;
        let dy = dab_y - self.last_dab_y;
        self.last_dab_x = dab_x;
        self.last_dab_y = dab_y;

        let dab_color = color.as_rounded_argb32();
        let dab_lock_alpha = if self.erase {
            0u8 // Erasing and locking alpha are at odds with each other.
        } else if self.lock_alpha {
            255u8
        } else {
            (lock_alpha * 255.0).clamp(0.0, 255.0) as u8
        };

        if self.needs_new_dab(dx, dy, dab_color, dab_lock_alpha) {
            self.dabs.push(DrawDabsMyPaintMessage {
                layer: self.layer_id,
                x: dab_x,
                y: dab_y,
                color: dab_color,
                lock_alpha: dab_lock_alpha,
                dabs: vec![dab],
            });
        } else {
            dab.x = dx as i8;
            dab.y = dy as i8;
            self.dabs.last_mut().unwrap().dabs.push(dab);
        }
    }
}

pub struct MyPaintBrushState {
    brush: *mut c::MyPaintBrush,
    in_progress: bool,
    target: MyPaintBrushStateTarget,
}

impl MyPaintBrushState {
    pub fn new() -> Self {
        MyPaintBrushState {
            brush: unsafe { new_mypaint_brush() },
            in_progress: false,
            target: MyPaintBrushStateTarget {
                layer_id: 0,
                lock_alpha: false,
                erase: false,
                dabs: Vec::new(),
                last_dab_x: 0,
                last_dab_y: 0,
            },
        }
    }

    fn set_base_value(&mut self, setting_id: c::MyPaintBrushSetting, value: f32) {
        unsafe { c::mypaint_brush_set_base_value(self.brush, setting_id, value) }
    }

    fn set_mapping_n(
        &mut self,
        setting_id: c::MyPaintBrushSetting,
        input_id: c::MyPaintBrushInput,
        n: i32,
    ) {
        unsafe { c::mypaint_brush_set_mapping_n(self.brush, setting_id, input_id, n) }
    }

    fn set_mapping_point(
        &mut self,
        setting_id: c::MyPaintBrushSetting,
        input_id: c::MyPaintBrushInput,
        n: i32,
        x: f32,
        y: f32,
    ) {
        unsafe { c::mypaint_brush_set_mapping_point(self.brush, setting_id, input_id, n, x, y) }
    }

    pub fn set_brush(&mut self, brush: &MyPaintBrush, settings: &MyPaintSettings, freehand: bool) {
        let mappings = &settings.mappings;
        for (i, mapping) in mappings.iter().enumerate() {
            let setting_id = i as c::MyPaintBrushSetting;
            self.set_base_value(setting_id, mapping.base_value);
            for j in 0..mapping.inputs.len() {
                let input = &mapping.inputs[j];
                let input_id = j as c::MyPaintBrushInput;
                self.set_mapping_n(setting_id, input_id, input.n);
                for n in 0..input.n {
                    self.set_mapping_point(
                        setting_id,
                        input_id,
                        n,
                        input.xvalues[n as usize],
                        input.yvalues[n as usize],
                    );
                }
            }
        }

        let (h, s, v) = brush.color.as_hsv();
        self.set_base_value(
            c::MyPaintBrushSetting_MYPAINT_BRUSH_SETTING_COLOR_H,
            h / 360.0,
        );
        self.set_base_value(c::MyPaintBrushSetting_MYPAINT_BRUSH_SETTING_COLOR_S, s);
        self.set_base_value(c::MyPaintBrushSetting_MYPAINT_BRUSH_SETTING_COLOR_V, v);

        self.target.lock_alpha = brush.lock_alpha;
        self.target.erase = brush.erase;

        // Slow tracking interferes with automated drawing of lines, shapes et
        // cetera, so we turn it off when it's not a living creature drawing.
        if !freehand {
            self.set_base_value(
                c::MyPaintBrushSetting_MYPAINT_BRUSH_SETTING_SLOW_TRACKING,
                0.0,
            );
        }
    }
}

impl BrushState for MyPaintBrushState {
    fn set_layer(&mut self, layer_id: u16) {
        self.target.layer_id = layer_id;
    }

    fn stroke_to(&mut self, x: f32, y: f32, p: f32, delta_msec: i64, source: Option<&BitmapLayer>) {
        let mut surface = MyPaintSurface::new(source, &mut self.target);
        if !self.in_progress {
            self.in_progress = true;
            unsafe {
                c::mypaint_brush_new_stroke(self.brush);
                // Generate a phantom stroke point with zero pressure and a
                // really high delta time to separate this stroke from the last.
                // This is because libmypaint actually expects you to transmit
                // strokes even when the pen is in the air, which is not how
                // Drawpile works. Generating this singular pen-in-air event
                // makes it behave properly though. Or arguably better than in
                // MyPaint, since when you crank up the smoothing really high
                // there and move the pen really fast, you can get strokes from
                // when your pen wasn't on the tablet, which is just weird.
                c::mypaint_brush_stroke_to(
                    self.brush,
                    surface.get_raw(),
                    x,
                    y,
                    0.0,
                    0.0,
                    0.0,
                    1000.0,
                );
            }
        }
        let delta_sec = (delta_msec as f64) / 1000.0f64;
        unsafe {
            c::mypaint_brush_stroke_to(self.brush, surface.get_raw(), x, y, p, 0.0, 0.0, delta_sec);
        }
    }

    fn end_stroke(&mut self) {
        self.in_progress = false;
        unsafe { c::mypaint_brush_reset(self.brush) }
    }

    fn write_dabs(&mut self, user_id: u8, writer: &mut MessageWriter) {
        // TODO excessive copying. We should be able to preserve
        // the vector and not make temporary copies of the individual
        // messages.
        let dabs = mem::take(&mut self.target.dabs);
        for d in dabs {
            CommandMessage::DrawDabsMyPaint(user_id, d).write(writer);
        }
    }

    fn take_dabs(&mut self, user_id: u8) -> Vec<CommandMessage> {
        let dabs = mem::take(&mut self.target.dabs);
        dabs.into_iter()
            .map(|d| CommandMessage::DrawDabsMyPaint(user_id, d))
            .collect()
    }

    fn add_offset(&mut self, _x: f32, _y: f32) {}
}
