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

use core::cmp::{max, min};

#[derive(Eq, PartialEq, Debug, Clone, Copy)]
pub struct Rectangle {
    pub x: i32,
    pub y: i32,
    pub w: i32,
    pub h: i32,
}

impl Rectangle {
    pub fn new(x: i32, y: i32, w: i32, h: i32) -> Rectangle {
        assert!(w > 0 && h > 0);
        Rectangle { x, y, w, h }
    }

    pub fn tile(x: i32, y: i32, size: i32) -> Rectangle {
        assert!(size > 0);
        Rectangle {
            x: x * size,
            y: y * size,
            w: size,
            h: size,
        }
    }

    pub fn contains(&self, other: &Rectangle) -> bool {
        self.x <= other.x
            && self.y <= other.y
            && self.right() >= other.right()
            && self.bottom() >= other.bottom()
    }

    pub fn intersected(&self, other: &Rectangle) -> Option<Rectangle> {
        let leftx = max(self.x, other.x);
        let rightx = min(self.x + self.w, other.x + other.w);
        let topy = max(self.y, other.y);
        let btmy = min(self.y + self.h, other.y + other.h);

        if leftx < rightx && topy < btmy {
            Some(Rectangle::new(leftx, topy, rightx - leftx, btmy - topy))
        } else {
            None
        }
    }

    pub fn union(&self, other: &Rectangle) -> Rectangle {
        let x0 = min(self.x, other.x);
        let y0 = min(self.y, other.y);
        let x1 = max(self.right(), other.right());
        let y1 = max(self.bottom(), other.bottom());

        Rectangle {
            x: x0,
            y: y0,
            w: x1 - x0 + 1,
            h: y1 - y0 + 1,
        }
    }

    pub fn cropped(&self, w: u32, h: u32) -> Option<Rectangle> {
        assert!(w > 0 && h > 0);
        assert!(w <= i32::max_value() as u32 && h <= i32::max_value() as u32);

        self.intersected(&Rectangle::new(0, 0, w as i32, h as i32))
    }

    pub fn right(&self) -> i32 {
        self.x + self.w - 1
    }
    pub fn bottom(&self) -> i32 {
        self.y + self.h - 1
    }

    pub fn offset(&self, x: i32, y: i32) -> Rectangle {
        Rectangle {
            x: self.x + x,
            y: self.y + y,
            w: self.w,
            h: self.h,
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_intersection() {
        let r1 = Rectangle::new(0, 0, 100, 100);
        let r2 = Rectangle::new(-10, -10, 20, 20);
        let edge = Rectangle::new(99, 0, 10, 10);

        assert_eq!(r1.intersected(&r2), Some(Rectangle::new(0, 0, 10, 10)));
        assert_eq!(r1.intersected(&edge), Some(Rectangle::new(99, 0, 1, 10)));

        let touching = Rectangle::new(100, 100, 20, 20);
        let outside = Rectangle::new(200, 200, 10, 10);
        assert_eq!(r1.intersected(&touching), None);
        assert_eq!(r1.intersected(&outside), None);
    }

    #[test]
    fn test_union() {
        let r1 = Rectangle::new(0, 0, 100, 100);
        let r2 = Rectangle::new(-10, -10, 20, 20);
        assert_eq!(r1.union(&r2), Rectangle::new(-10, -10, 110, 110));

        let inside = Rectangle::new(10, 10, 10, 10);
        assert_eq!(r1.union(&inside), r1);
    }
}
