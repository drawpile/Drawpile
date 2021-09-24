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

use crate::paint::BitmapLayer;
use crate::protocol::message::CommandMessage;
use crate::protocol::MessageWriter;

pub trait BrushState {
    /// Set the target layer
    fn set_layer(&mut self, layer_id: u16);

    /// Draw a brush stroke to this point
    ///
    /// If there is no active stroke, this becomes the starting point
    ///
    /// If a source layer is given, it is used as the source of color smudging pixels
    fn stroke_to(&mut self, x: f32, y: f32, p: f32, source: Option<&BitmapLayer>);

    /// End the current stroke (if any)
    fn end_stroke(&mut self);

    /// Take the dabs computed so far and write them with the given message writer
    /// Doing this will not end the current stroke.
    fn write_dabs(&mut self, user_id: u8, writer: &mut MessageWriter);

    /// Take the dabs computed so far.
    /// Doing this will not end the current stroke.
    fn take_dabs(&mut self, user_id: u8) -> Vec<CommandMessage>;

    /// Add an offset to the current position.
    ///
    /// This is used to correct the active position when the viewport
    /// is moved while the user is still drawing.
    ///
    /// Does nothing if there is no stroke in progress.
    fn add_offset(&mut self, x: f32, y: f32);
}
