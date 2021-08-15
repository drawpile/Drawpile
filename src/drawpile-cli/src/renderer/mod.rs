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

use dpcore::canvas::CanvasState;
use dpcore::paint::color::*;
use dpcore::protocol::message::{CommandMessage, Message};
use dpcore::protocol::{open_recording, Compatibility, ReadMessage};

use tracing::{info, warn};

use std::error::Error;
use std::fmt;
use std::io;
use std::num::ParseIntError;
use std::str::FromStr;
use std::time::{Duration, Instant};

use image;

#[derive(Clone, Copy, PartialEq)]
pub struct Size(u32, u32);

impl FromStr for Size {
    type Err = ParseIntError;
    fn from_str(s: &str) -> Result<Self, Self::Err> {
        let x = s.find('x').unwrap_or(0);
        let w = s[..x].parse::<u32>()?;
        let h = s[x + 1..].parse::<u32>()?;
        Ok(Size(w, h))
    }
}

pub struct RenderOpts<'a> {
    /// Name of input recording file
    pub input_file: &'a str,

    /// Name of output image file (sequence number is inserted before the suffix)
    pub output_file: &'a str,

    /// How often to write the image
    pub output_every: Option<u32>,

    /// Whether to write a message or an undopoint
    pub every_up: bool,

    /// Resize images to this size
    pub resize: Option<Size>,

    /// Resize subsequent images to the original size
    pub same_size: bool,
}

struct RenderState {
    resize: Option<Size>,
    same_size: bool,
    image_num: u32,
}

#[derive(Debug)]
struct RenderError {
    message: &'static str,
}

impl fmt::Display for RenderError {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "{}", self.message)
    }
}

impl Error for RenderError {
    fn description(&self) -> &str {
        &self.message
    }
}

pub fn render_recording(opts: &RenderOpts) -> Result<(), Box<dyn std::error::Error>> {
    let mut reader = open_recording(opts.input_file)?;

    if reader.check_compatibility() == Compatibility::Incompatible {
        return Err(Box::new(RenderError {
            message: "Unsupported format version",
        }));
    }

    let start = Instant::now();
    let mut canvas = CanvasState::new();
    let mut total_render_time = Duration::new(0, 0);
    let mut total_save_time = Duration::new(0, 0);
    let mut message_counter = 0;
    let mut state = RenderState {
        resize: opts.resize,
        same_size: opts.same_size,
        image_num: opts.output_every.map_or(0, |_| 1),
    };

    loop {
        match reader.read_next() {
            ReadMessage::Ok(m) => {
                let now = Instant::now();
                match &m {
                    Message::Command(c) => {
                        match (opts.every_up, c) {
                            (true, CommandMessage::UndoPoint(_)) | (false, _) => {
                                message_counter += 1;
                            }
                            _ => (),
                        }

                        canvas.receive_message(c);
                    }
                    _ => (),
                }
                total_render_time += now.elapsed();

                if let Some(e) = opts.output_every {
                    if message_counter >= e {
                        message_counter = 0;
                        total_save_time += save_canvas(&opts, &mut state, &canvas)?;
                    }
                }
            }
            ReadMessage::Invalid(msg) => {
                warn!("Invalid message: {}", msg);
            }
            ReadMessage::IoError(e) => {
                return Err(Box::new(e));
            }
            ReadMessage::Eof => {
                break;
            }
        }
    }

    total_save_time += save_canvas(&opts, &mut state, &canvas)?;

    let total_time = start.elapsed();

    info!(
        "Total render time: {:.3} s",
        total_render_time.as_secs_f64()
    );
    info!("Total save time: {:.3} s", total_save_time.as_secs_f64());
    info!("Total time: {:.3} s", total_time.as_secs_f64());

    Ok(())
}

fn save_canvas(
    opts: &RenderOpts,
    state: &mut RenderState,
    canvas: &CanvasState,
) -> io::Result<Duration> {
    let now = Instant::now();

    let filename = make_filename(opts, state.image_num);

    let img = canvas.layerstack().to_image();
    let mut rgba = Vec::<u8>::with_capacity(img.width * img.height * 4);
    for px in img.pixels.iter() {
        rgba.push(px[RED_CHANNEL]);
        rgba.push(px[GREEN_CHANNEL]);
        rgba.push(px[BLUE_CHANNEL]);
        rgba.push(px[ALPHA_CHANNEL]);
    }
    let mut ib = image::RgbaImage::from_raw(img.width as u32, img.height as u32, rgba).unwrap();

    let size = Size(img.width as u32, img.height as u32);

    if let Some(resize) = state.resize {
        if size != resize {
            ib = image::imageops::resize(&ib, resize.0, resize.1, image::FilterType::CatmullRom);
        }
    } else if state.same_size {
        state.resize = Some(size);
    }

    ib.save(&filename)?;
    info!("Saved {}", filename);

    state.image_num += 1;
    Ok(now.elapsed())
}

fn make_filename(opts: &RenderOpts, index: u32) -> String {
    if opts.output_file == "" {
        let end = opts.input_file.rfind('.').unwrap_or(opts.input_file.len());

        if index != 0 {
            format!("{}-{}.png", &opts.input_file[..end], index)
        } else {
            format!("{}.png", &opts.input_file[..end])
        }
    } else {
        if index != 0 {
            let suffix = opts
                .output_file
                .rfind('.')
                .unwrap_or(opts.output_file.len());
            format!(
                "{}-{}{}",
                &opts.input_file[..suffix],
                index,
                &opts.output_file[suffix..]
            )
        } else {
            opts.output_file.to_string()
        }
    }
}
