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

use clap::{ArgGroup, Parser, Subcommand};
use tracing::Level;

use drawpile_cli::converter::*;
use drawpile_cli::indexer::{decode_index, extract_snapshot, index_recording};
use drawpile_cli::renderer::*;

#[derive(Parser)]
#[clap(version, about)]
struct Cli {
    #[clap(subcommand)]
    command: Commands,
}

#[derive(Subcommand)]
enum Commands {
    /// Convert between binary and text formats
    Convert {
        /// Input file
        #[clap(value_parser)]
        input: String,

        /// Output file
        #[clap(value_parser)]
        output: Option<String>,

        /// Output format
        #[clap(value_parser, short, long)]
        format: Option<Format>,
    },
    /// Render a recording
    #[clap(
        group(ArgGroup::new("every").args(&["every-msg", "every-up"])),
        group(ArgGroup::new("size").args(&["resize", "same-size"]))
    )]
    Render {
        /// Input file
        #[clap(value_parser)]
        input: String,

        /// Output file
        #[clap(value_parser)]
        output: Option<String>,

        /// Save image every n messages
        #[clap(short, long, value_parser)]
        every_msg: Option<u32>,

        /// Save image every n undo points
        #[clap(long, value_parser)]
        every_up: Option<u32>,

        /// Resize canvas to this size (WxH)
        #[clap(long, value_parser)]
        resize: Option<Size>,

        /// Resize subsequent images to original size
        #[clap(short, long)]
        same_size: bool,
    },
    /// Build an index for a recording
    Index {
        /// Input file
        #[clap(value_parser)]
        input: String,
    },
    /// Decode a recording index
    DecodeIndex {
        /// Input file
        #[clap(value_parser)]
        input: String,

        /// Extract snapshot at the given index
        #[clap(short='x', long, value_name="INDEX")]
        extract: Option<usize>,
    }
}
fn main() -> Result<(), Box<dyn std::error::Error>> {
    tracing_subscriber::fmt::Subscriber::builder()
        .with_max_level(Level::DEBUG)
        .init();

    let cli = Cli::parse();

    match cli.command {
        Commands::Convert {
            input,
            output,
            format,
        } => {
            let opts = ConvertRecOpts {
                input_file: &input,
                output_file: output.as_deref().unwrap_or("-"),
                output_format: format.unwrap_or_default(),
            };

            convert_recording(&opts)
        }
        Commands::Render {
            input,
            output,
            every_msg,
            every_up,
            resize,
            same_size,
        } => {
            let opts = RenderOpts {
                input_file: &input,
                output_file: output.as_deref().unwrap_or_default(),
                output_every: every_msg.or(every_up),
                every_up: every_up.is_some(),
                resize,
                same_size,
            };

            render_recording(&opts)
        }
        Commands::Index { input } => {
            index_recording(&input)
        }
        Commands::DecodeIndex { input, extract } => {
            if let Some(idx) = extract {
                extract_snapshot(&input, idx)
            } else {
                decode_index(&input)
            }
        }
    }
}
