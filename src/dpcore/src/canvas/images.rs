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

use super::compression::compress_image;
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
    // TODO this could be implemented with just image rect iterators to avoid unnecessary copies
    let compressed = compress_image(&image.pixels);

    if compressed.len() > MAX_IMAGE_LEN {
        // Image too big to fit into a message. Split into four pieces and
        // try again
        // TODO: a smarter approach would be to split at tile boundaries to
        // make the PutImage operations more efficient
        let splitx = (image.width / 2) as i32;
        let splity = (image.height / 2) as i32;

        let mut cmds: Vec<CommandMessage> = Vec::with_capacity(4);

        cmds.append(&mut make_putimage(
            user,
            layer,
            x,
            y,
            &image.cropped(&Rectangle::new(0, 0, splitx, splity)),
            mode,
        ));
        cmds.append(&mut make_putimage(
            user,
            layer,
            x + splitx as u32,
            y,
            &image.cropped(&Rectangle::new(
                splitx,
                0,
                image.width as i32 - splitx,
                splity,
            )),
            mode,
        ));
        cmds.append(&mut make_putimage(
            user,
            layer,
            x,
            y + splity as u32,
            &image.cropped(&Rectangle::new(
                0,
                splity,
                splitx,
                image.height as i32 - splity,
            )),
            mode,
        ));
        cmds.append(&mut make_putimage(
            user,
            layer,
            x + splitx as u32,
            y + splity as u32,
            &image.cropped(&Rectangle::new(
                splitx,
                0,
                image.width as i32 - splitx,
                image.height as i32 - splity,
            )),
            mode,
        ));
        cmds
    } else {
        vec![CommandMessage::PutImage(
            user,
            PutImageMessage {
                layer,
                mode: mode.into(),
                x,
                y,
                w: image.width as u32,
                h: image.height as u32,
                image: compressed,
            },
        )]
    }
}
