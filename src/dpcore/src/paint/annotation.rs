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

use super::{Color, Rectangle};

pub type AnnotationID = u16;

/// A floating text box over the image
///
/// These are not strictly part of the canvas pixel data,
/// but they belong to the layerstack.
///
/// The core paint engine has no opinion on how annotations
/// are rasterized. To merge an annotation, it must be converted
/// to a bitmap on the client side, using fonts available there,
/// then merged using the PutImage command.
#[derive(Clone)]
pub struct Annotation {
    pub id: AnnotationID,
    pub text: String,
    pub rect: Rectangle,
    pub background: Color,
    pub protect: bool,
    pub valign: VAlign,
}

#[derive(Clone, Copy)]
pub enum VAlign {
    Top,
    Center,
    Bottom,
}
