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

use super::color::{Pixel15, Pixel8};
use super::rectiter::{MutableRectIterator, RectIterator};
use super::{Rectangle, Size};

/// A flat image buffer
#[derive(Default)]
pub struct Image<T>
where
    T: Clone + Default + Eq,
{
    pub pixels: Vec<T>,
    pub width: usize,
    pub height: usize,
}

pub type Image8 = Image<Pixel8>;
pub type Image15 = Image<Pixel15>;

impl<T> Image<T>
where
    T: Clone + Default + Eq,
{
    pub fn new(width: usize, height: usize) -> Image<T> {
        Image {
            pixels: vec![T::default(); width * height],
            width,
            height,
        }
    }

    pub fn is_null(&self) -> bool {
        assert!(self.pixels.len() == self.width * self.height);
        self.pixels.is_empty()
    }

    pub fn size(&self) -> Size {
        Size::new(self.width as i32, self.height as i32)
    }

    /// Find the bounding rectangle of the opaque pixels in this image
    /// If the image is entirely transparent, None is returned.
    pub fn opaque_bounds(&self) -> Option<Rectangle> {
        assert!(self.pixels.len() == self.width * self.height);
        if self.pixels.is_empty() {
            return None;
        }

        let mut top = self.height;
        let mut btm = 0;
        let mut left = self.width;
        let mut right = 0;

        let zero_pixel = T::default();
        for y in 0..self.height {
            let row = y * self.width;
            for (x, px) in self.pixels[row..row + self.width].iter().enumerate() {
                if *px != zero_pixel {
                    left = left.min(x);
                    right = right.max(x);
                    top = top.min(y);
                    btm = btm.max(y);
                }
            }
        }

        if top > btm {
            return None;
        }

        Some(Rectangle {
            x: left as i32,
            y: top as i32,
            w: (right - left + 1) as i32,
            h: (btm - top + 1) as i32,
        })
    }

    pub fn rect_iter(&self, rect: &Rectangle) -> RectIterator<T> {
        RectIterator::from_rectangle(&self.pixels, self.width, rect)
    }

    pub fn rect_iter_mut(&mut self, rect: &Rectangle) -> MutableRectIterator<T> {
        MutableRectIterator::from_rectangle(&mut self.pixels, self.width, rect)
    }

    /// Return a cropped version of the image
    pub fn cropped(&self, rect: &Rectangle) -> Image<T> {
        let rect = match rect.cropped(self.size()) {
            Some(r) => r,
            None => return Image::default(),
        };

        let mut cropped_pixels: Vec<T> = Vec::with_capacity((rect.w * rect.h) as usize);
        self.rect_iter(&rect)
            .for_each(|p| cropped_pixels.extend_from_slice(p));

        Image {
            pixels: cropped_pixels,
            width: rect.w as usize,
            height: rect.h as usize,
        }
    }
}
