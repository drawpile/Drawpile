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

use crate::converter::ConversionError;
use dpcore::protocol::message::Message;
use dpimpex::rec::reader::{open_recording, Compatibility, ReadMessage};
use dpimpex::rec_index::reader as index_reader;
use dpimpex::rec_index::writer::{IndexBuilder, Stats};
use dpimpex::save_image;

use std::fs::File;
use std::path::Path;
use std::time::{Duration, Instant};

pub fn index_recording(input_file: &str) -> Result<(), Box<dyn std::error::Error>> {
    let input_path = Path::new(input_file);
    let mut reader = open_recording(input_path)?;

    if reader.check_compatibility() == Compatibility::Incompatible {
        return Err(Box::new(ConversionError {
            message: "Unsupported format version",
        }));
    }

    let mut indexer = IndexBuilder::new(File::create(input_path.with_extension("dpidx"))?);

    indexer.write_header()?;
    let mut last_snapshot = Instant::now();
    let mut thumbnail_counter: i32 = 0;

    loop {
        let offset = reader.current_offset();

        match reader.read_next() {
            ReadMessage::Ok(Message::Command(m)) => {
                indexer.add_message(&m, offset);

                thumbnail_counter -= 1;
                if last_snapshot.elapsed() > Duration::from_millis(100) {
                    let stats = indexer.make_entry(thumbnail_counter <= 0)?;
                    print_stats(&stats, thumbnail_counter);

                    if thumbnail_counter <= 0 {
                        thumbnail_counter = 1000;
                    }

                    last_snapshot = Instant::now();
                }
            }
            ReadMessage::Ok(_) => (),
            ReadMessage::Invalid(msg) => {
                eprintln!("Invalid message: {msg}");
            }
            ReadMessage::IoError(e) => {
                return Err(Box::new(e));
            }
            ReadMessage::Eof => {
                let stats = indexer.make_entry(thumbnail_counter <= 0)?;
                print_stats(&stats, thumbnail_counter);
                break;
            }
        }
    }

    indexer.finalize()?;
    Ok(())
}

fn print_stats(stats: &Stats, thumbnail_counter: i32) {
    println!(
        "{}: tiles(new: {}, reused: {} [{:.1}%], null: {}), layers(new: {}, reused: {}), annotations(new: {}, reused: {}){}{}{}",
        stats.index,
        stats.new_tiles,
        stats.reused_tiles,
        if stats.new_tiles > 0 || stats.reused_tiles > 0 { stats.reused_tiles as f32 / (stats.reused_tiles + stats.new_tiles) as f32 * 100.0 } else { 0.0 },
        stats.null_tiles,
        stats.changed_layers,
        stats.reused_layers,
        stats.changed_annotations,
        stats.reused_annotations,
        if stats.metadata_changed { ", metadata changed" } else { "" },
        if stats.timeline_changed { ", timeline changed" } else { "" },
        if thumbnail_counter <= 0 { ", thumbnail" } else { "" },
    );
}

pub fn decode_index(input_file: &str) -> Result<(), Box<dyn std::error::Error>> {
    let index = index_reader::read_index(&mut File::open(input_file)?)?;

    println!("Indexed {} messages", index.messages);
    for (i, entry) in index.entries.iter().enumerate() {
        println!(
            "{}: message {} at {}, snapshot at {}, thumbnail at {}",
            i,
            entry.message_index,
            entry.message_offset,
            entry.snapshot_offset,
            entry.thumbnail_offset,
        );
    }

    Ok(())
}

pub fn extract_snapshot(
    input_file: &str,
    index_entry: usize,
) -> Result<(), Box<dyn std::error::Error>> {
    let mut reader = File::open(input_file)?;

    let index = index_reader::read_index(&mut reader)?;

    let layerstack = index_reader::read_snapshot(&mut reader, &index.entries[index_entry])?;

    save_image("snapshot.png", &layerstack)?;

    Ok(())
}
