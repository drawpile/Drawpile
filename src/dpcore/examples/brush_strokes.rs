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

use dpcore::brush::{BrushEngine, BrushState, ClassicBrush, ClassicBrushShape};
use dpcore::canvas::CanvasState;
use dpcore::paint::Color;
use dpcore::protocol::message::{CommandMessage, Message};
use std::f32::consts::PI;

mod utils;

fn main() {
    let mut canvas = CanvasState::new();

    canvas.receive_message(&cmd("1 resize right=1024 bottom=256"));
    canvas.receive_message(&cmd("1 newlayer id=0x0100 fill=#ffffff"));
    canvas.receive_message(&cmd("1 newlayer id=0x0101"));

    let mut painter = BrushEngine::new();
    painter.set_layer(0x0101);

    // Pixel brush wavy line
    painter.set_classicbrush(ClassicBrush {
        size: (1.0, 10.0).into(),
        size_pressure: true,
        ..ClassicBrush::default()
    });

    draw_wavy_line(&mut painter, 10.0, 50.0, 512.0 - 20.0, 40.0);
    painter.take_dabs(1).iter().for_each(|d| {
        canvas.receive_message(d);
    });

    // Pixel brush wavy (indirect)
    painter.set_classicbrush(ClassicBrush {
        size: (1.0, 10.0).into(),
        opacity: (0.0, 0.5).into(),
        color: Color::rgb8(255, 0, 0),
        incremental: false,
        opacity_pressure: true,
        ..ClassicBrush::default()
    });

    draw_wavy_line(&mut painter, 10.0, 90.0, 512.0 - 20.0, 40.0);
    painter.take_dabs(1).iter().for_each(|d| {
        canvas.receive_message(d);
    });
    canvas.receive_message(&cmd("1 penup"));

    // Pixel brush straight line
    painter.set_classicbrush(ClassicBrush::default());
    painter.stroke_to(10.0, 140.0, 1.0, 0, None);
    painter.stroke_to(512.0 - 20.0, 150.0, 1.0, 0, None);
    painter.end_stroke();
    painter.take_dabs(1).iter().for_each(|d| {
        canvas.receive_message(d);
    });

    // Pixel brush dotted wave
    painter.set_classicbrush(ClassicBrush {
        size: (1.0, 10.0).into(),
        spacing: 1.5,
        color: Color::rgb8(0, 255, 0),
        shape: ClassicBrushShape::SquarePixel,
        ..ClassicBrush::default()
    });

    draw_wavy_line(&mut painter, 10.0, 180.0, 512.0 - 20.0, 20.0);
    painter.take_dabs(1).iter().for_each(|d| {
        canvas.receive_message(d);
    });

    // Pixel brush smudging mode
    painter.set_classicbrush(ClassicBrush {
        size: (1.0, 10.0).into(),
        smudge: (0.0, 0.5).into(),
        color: Color::rgb8(0, 0, 255),
        ..ClassicBrush::default()
    });
    painter.stroke_to(
        30.0,
        10.0,
        1.0,
        0,
        canvas.layerstack().root().get_bitmaplayer(0x0101),
    );
    painter.stroke_to(
        30.0,
        200.0,
        1.0,
        0,
        canvas.layerstack().root().get_bitmaplayer(0x0101),
    );
    painter.end_stroke();
    painter.take_dabs(1).iter().for_each(|d| {
        canvas.receive_message(d);
    });

    // Soft brush

    // Soft brush wavy line
    painter.set_classicbrush(ClassicBrush {
        size: (1.0, 10.0).into(),
        shape: ClassicBrushShape::RoundSoft,
        size_pressure: true,
        ..ClassicBrush::default()
    });

    draw_wavy_line(&mut painter, 522.0, 50.0, 512.0 - 20.0, 40.0);
    painter.take_dabs(1).iter().for_each(|d| {
        canvas.receive_message(d);
    });

    // Soft brush wavy (indirect)
    painter.set_classicbrush(ClassicBrush {
        size: (1.0, 10.0).into(),
        opacity: (0.0, 0.5).into(),
        color: Color::rgb8(255, 0, 0),
        incremental: false,
        opacity_pressure: true,
        shape: ClassicBrushShape::RoundSoft,
        ..ClassicBrush::default()
    });

    draw_wavy_line(&mut painter, 522.0, 90.0, 512.0 - 20.0, 40.0);
    painter.take_dabs(1).iter().for_each(|d| {
        canvas.receive_message(d);
    });
    canvas.receive_message(&cmd("1 penup"));

    // Soft brush straight line
    painter.set_classicbrush(ClassicBrush {
        shape: ClassicBrushShape::RoundSoft,
        ..ClassicBrush::default()
    });
    painter.stroke_to(522.0, 140.0, 1.0, 0, None);
    painter.stroke_to(1024.0 - 20.0, 150.0, 1.0, 0, None);
    painter.end_stroke();
    painter.take_dabs(1).iter().for_each(|d| {
        canvas.receive_message(d);
    });

    // Soft brush dotted wave
    painter.set_classicbrush(ClassicBrush {
        size: (1.0, 10.0).into(),
        spacing: 1.5,
        color: Color::rgb8(0, 255, 0),
        shape: ClassicBrushShape::RoundSoft,
        hardness_pressure: true,
        ..ClassicBrush::default()
    });

    draw_wavy_line(&mut painter, 522.0, 180.0, 512.0 - 20.0, 20.0);
    painter.take_dabs(1).iter().for_each(|d| {
        canvas.receive_message(d);
    });

    // Soft brush smudging mode
    painter.set_classicbrush(ClassicBrush {
        size: (1.0, 10.0).into(),
        smudge: (0.0, 0.5).into(),
        color: Color::rgb8(0, 0, 255),
        shape: ClassicBrushShape::RoundSoft,
        ..ClassicBrush::default()
    });
    painter.stroke_to(
        542.0,
        10.0,
        1.0,
        0,
        canvas.layerstack().root().get_bitmaplayer(0x0101),
    );
    painter.stroke_to(
        542.0,
        200.0,
        1.0,
        0,
        canvas.layerstack().root().get_bitmaplayer(0x0101),
    );
    painter.end_stroke();
    painter.take_dabs(1).iter().for_each(|d| {
        canvas.receive_message(d);
    });

    utils::save_layerstack(canvas.layerstack(), "example_brush_strokes.png");
}

fn draw_wavy_line(
    brush: &mut impl BrushState,
    start_x: f32,
    start_y: f32,
    width: f32,
    amplitude: f32,
) {
    let mut x = 0.0;
    while x < width {
        let y = (x / width * PI * 4.0).sin() * amplitude;
        let p = (x / width * PI).sin();
        brush.stroke_to(start_x + x, start_y + y, p, 0, None);
        x += 1.0;
    }
    brush.end_stroke();
}

fn cmd(msg: &str) -> CommandMessage {
    match Message::from_text(&msg.parse().unwrap()).unwrap() {
        Message::Command(m) => m,
        _ => panic!("Not a command message: {}", msg),
    }
}
