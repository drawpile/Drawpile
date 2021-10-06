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

use super::compression::ImageCompressor;
use crate::paint::{Blendmode, Image, LayerID, Rectangle, UserID};
use crate::protocol::message::{CommandMessage, PutImageMessage};

const MAX_IMAGE_LEN: usize = 0xffff - 19;


/// Make a PutImage command.
///
/// If the image is too big to fit into a single command,
/// multiple PutImages will be written.
pub fn make_putimage(
    user: UserID,
    layer: LayerID,
    x: u32,
    y: u32,
    image: &Image,
    mode: Blendmode,
) -> Vec<CommandMessage> {
    let mut messages = Vec::new();
    make_putimage_crop(
        &mut messages,
        user,
        layer,
        x,
        y,
        image,
        mode,
        &Rectangle::new(0, 0, image.width as i32, image.height as i32)
    );
    messages
}

fn make_putimage_crop(
    messages: &mut Vec<CommandMessage>,
    user: UserID,
    layer: LayerID,
    x: u32,
    y: u32,
    image: &Image,
    mode: Blendmode,
    rect: &Rectangle,
) {
    // Try compressing this subregion of the image
    {
        let mut compressor = ImageCompressor::new();
        image.rect_iter(rect).for_each(|row| compressor.add(row));
        let compressed = compressor.finish();

        if compressed.len() <= MAX_IMAGE_LEN {
            messages.push(CommandMessage::PutImage(
                user,
                PutImageMessage {
                    layer,
                    mode: mode.into(),
                    x: x + rect.x as u32,
                    y: y + rect.y as u32,
                    w: rect.w as u32,
                    h: rect.h as u32,
                    image: compressed,
                },
            ));
            return;
        }
    }

    // Image too big to fit into a message. Split into four pieces and
    // try again
    // TODO: a smarter approach would be to split at tile boundaries to
    // make the PutImage operations more efficient
    let splitx = rect.w / 2;
    let splity = rect.h / 2;

    make_putimage_crop(
        messages,
        user,
        layer,
        x,
        y,
        image,
        mode,
        &Rectangle::new(rect.x, rect.y, splitx, splity),
    );
    make_putimage_crop(
        messages,
        user,
        layer,
        x,
        y,
        image,
        mode,
        &Rectangle::new(
            rect.x + splitx,
            rect.y,
            rect.w - splitx,
            splity,
        ),
    );
    make_putimage_crop(
        messages,
        user,
        layer,
        x,
        y,
        image,
        mode,
        &Rectangle::new(
            rect.x,
            rect.y + splity,
            splitx,
            rect.h - splity,
        ),
    );
    make_putimage_crop(
        messages,
        user,
        layer,
        x,
        y,
        image,
        mode,
        &Rectangle::new(
            rect.x + splitx,
            rect.y + splity,
            rect.w - splitx,
            rect.h - splity,
        ),
    );
}
