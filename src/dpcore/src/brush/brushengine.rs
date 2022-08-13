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
use super::classicbrush::ClassicBrush;
use super::mypaintbrush::{MyPaintBrush, MyPaintSettings};
use super::mypaintbrushstate::MyPaintBrushState;
use super::pixelbrushstate::PixelBrushState;
use super::softbrushstate::SoftBrushState;
use crate::paint::BitmapLayer;
use crate::protocol::message::CommandMessage;
use crate::protocol::MessageWriter;

pub struct BrushEngine {
    pixel: PixelBrushState,
    soft: SoftBrushState,
    mypaint: MyPaintBrushState,
    active: ActiveBrush,
}

enum ActiveBrush {
    ClassicPixel,
    ClassicSoft,
    MyPaint,
}

impl BrushEngine {
    pub fn new() -> BrushEngine {
        BrushEngine {
            pixel: PixelBrushState::new(),
            soft: SoftBrushState::new(),
            mypaint: MyPaintBrushState::new(),
            active: ActiveBrush::ClassicPixel,
        }
    }

    pub fn set_classicbrush(&mut self, brush: ClassicBrush) {
        if brush.is_pixelbrush() {
            self.pixel.set_brush(brush);
            self.active = ActiveBrush::ClassicPixel;
        } else {
            self.soft.set_brush(brush);
            self.active = ActiveBrush::ClassicSoft;
        }
    }

    pub fn set_mypaintbrush(
        &mut self,
        brush: &MyPaintBrush,
        settings: &MyPaintSettings,
        freehand: bool,
    ) {
        self.mypaint.set_brush(brush, settings, freehand);
        self.active = ActiveBrush::MyPaint;
    }

    fn state(&mut self) -> &mut dyn BrushState {
        match self.active {
            ActiveBrush::ClassicPixel => &mut self.pixel,
            ActiveBrush::ClassicSoft => &mut self.soft,
            ActiveBrush::MyPaint => &mut self.mypaint,
        }
    }
}

impl BrushState for BrushEngine {
    fn set_layer(&mut self, layer_id: u16) {
        self.pixel.set_layer(layer_id);
        self.soft.set_layer(layer_id);
        self.mypaint.set_layer(layer_id);
    }

    fn stroke_to(&mut self, x: f32, y: f32, p: f32, delta_msec: i64, source: Option<&BitmapLayer>) {
        self.state().stroke_to(x, y, p, delta_msec, source);
    }

    fn end_stroke(&mut self) {
        self.state().end_stroke();
    }

    fn take_dabs(&mut self, user_id: u8) -> Vec<CommandMessage> {
        self.state().take_dabs(user_id)
    }

    fn write_dabs(&mut self, user_id: u8, writer: &mut MessageWriter) {
        self.state().write_dabs(user_id, writer);
    }

    fn add_offset(&mut self, x: f32, y: f32) {
        self.state().add_offset(x, y);
    }
}

impl Default for BrushEngine {
    fn default() -> Self {
        Self::new()
    }
}
