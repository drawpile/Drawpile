// This file is part of Drawpile.
// Copyright (C) 2020-2021 Calle Laakkonen
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

use clap::{value_t, App, Arg, Error as ClapError};
use tracing::Level;
use tracing_subscriber;

use drawpile_cli::converter::*;
use drawpile_cli::indexer::{decode_index, extract_snapshot, index_recording};
use drawpile_cli::renderer::*;

fn main() -> Result<(), Box<dyn std::error::Error>> {
    tracing_subscriber::fmt::Subscriber::builder()
        .with_max_level(Level::DEBUG)
        .init();

    let matches = App::new("Drawpile-cli")
        .version("0.1.0")
        .about("Convert Drawpile recordings")
        .subcommand(
            App::new("convert")
                .arg(Arg::with_name("INPUT").help("Input file").required(true))
                .arg(Arg::with_name("OUTPUT").help("Output file"))
                .arg(
                    Arg::with_name("format")
                        .short("f")
                        .long("format")
                        .help("Output format")
                        .takes_value(true),
                ),
        )
        .subcommand(
            App::new("render")
                .arg(Arg::with_name("INPUT").help("Input file").required(true))
                .arg(Arg::with_name("OUTPUT").help("Output file"))
                .arg(
                    Arg::with_name("every-msg")
                        .long("every-msg")
                        .takes_value(true)
                        .help("Save image every n messages"),
                )
                .arg(
                    Arg::with_name("every-up")
                        .long("every-up")
                        .takes_value(true)
                        .help("Save image every n undopoints")
                        .conflicts_with("every-msg"),
                )
                .arg(
                    Arg::with_name("resize")
                        .long("resize")
                        .takes_value(true)
                        .help("Resize canvas to this size (WxH)"),
                )
                .arg(
                    Arg::with_name("same-size")
                        .long("same-size")
                        .conflicts_with("resize")
                        .help("Resize subsequent images to the original size"),
                ),
        )
        .subcommand(
            App::new("index").arg(Arg::with_name("INPUT").help("Input file").required(true)),
        )
        .subcommand(
            App::new("decode-index")
                .arg(Arg::with_name("INPUT").help("Input file").required(true))
                .arg(
                    Arg::with_name("entry")
                        .long("extract")
                        .short("x")
                        .takes_value(true)
                        .help("Extract snapshot at index"),
                ),
        )
        .get_matches();

    match matches.subcommand() {
        ("convert", Some(m)) => {
            let opts = ConvertRecOpts {
                input_file: m.value_of("INPUT").unwrap(),
                output_file: m.value_of("OUTPUT").unwrap_or("-"),
                output_format: Format::Guess,
            };

            convert_recording(&opts)
        }
        ("render", Some(m)) => {
            let opts = RenderOpts {
                input_file: m.value_of("INPUT").unwrap(),
                output_file: m.value_of("OUTPUT").unwrap_or(""),
                output_every: m.value_of("every-msg").or(m.value_of("every-up")).map(|v| {
                    match v.parse::<u32>() {
                        Ok(val) => val,
                        Err(_) => {
                            ClapError::value_validation_auto(format!("{}: Not a number", v)).exit()
                        }
                    }
                }),
                every_up: m.value_of("every-up").is_some(),
                resize: if m.is_present("resize") {
                    Some(value_t!(m, "resize", Size).unwrap_or_else(|e| e.exit()))
                } else {
                    None
                },
                same_size: m.is_present("same-size"),
            };

            render_recording(&opts)
        }
        ("index", Some(m)) => index_recording(m.value_of("INPUT").unwrap()),
        ("decode-index", Some(m)) => {
            if m.is_present("entry") {
                extract_snapshot(
                    m.value_of("INPUT").unwrap(),
                    m.value_of("entry").unwrap().parse::<usize>()?,
                )
            } else {
                decode_index(m.value_of("INPUT").unwrap())
            }
        }
        ("", None) => {
            println!("Use: drawpile-cli convert or drawpile-cli render");
            Ok(())
        }
        _ => unreachable!(),
    }
}
