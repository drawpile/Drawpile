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

use super::Rectangle;

pub struct RectIterator<'a, T> {
    buf: &'a [T],
    stride: usize,
    x0: usize,
    x1: usize,
    rows: usize,
}

fn test_bounds(buflen: usize, stride: usize, x: usize, y: usize, w: usize, h: usize) {
    // The iterator itself is safe even if these preconditions are not met
    debug_assert!(w > 0);
    debug_assert!(h > 0);
    debug_assert!(x + w <= stride);
    debug_assert!(((y + h - 1) * stride + x + w) <= buflen);
}

impl<'a, T> RectIterator<'a, T> {
    pub fn new(
        buf: &'a [T],
        stride: usize,
        x: usize,
        y: usize,
        w: usize,
        h: usize,
    ) -> RectIterator<'a, T> {
        test_bounds(buf.len(), stride, x, y, w, h);
        RectIterator {
            buf: &buf[(y * stride)..],
            stride,
            x0: x,
            x1: x + w,
            rows: h,
        }
    }

    pub fn from_rectangle(buf: &'a [T], stride: usize, r: &Rectangle) -> RectIterator<'a, T> {
        RectIterator::new(
            buf,
            stride,
            r.x as usize,
            r.y as usize,
            r.w as usize,
            r.h as usize,
        )
    }

    pub fn area(&self) -> usize {
        self.rows * (self.x1 - self.x0)
    }
}

impl<'a, T> Iterator for RectIterator<'a, T> {
    type Item = &'a [T];
    fn next(&mut self) -> Option<Self::Item> {
        if self.rows > 0 {
            self.rows -= 1;
            let slice = &self.buf[self.x0..self.x1];
            self.buf = &self.buf[self.stride..];
            Some(slice)
        } else {
            None
        }
    }

    fn size_hint(&self) -> (usize, Option<usize>) {
        (self.rows, Some(self.rows))
    }
}

pub struct MutableRectIterator<'a, T: 'a> {
    buf: &'a mut [T],
    stride: usize,
    x0: usize,
    x1: usize,
    rows: usize,
}

impl<'a, T> MutableRectIterator<'a, T> {
    pub fn new(
        buf: &'a mut [T],
        stride: usize,
        x: usize,
        y: usize,
        w: usize,
        h: usize,
    ) -> MutableRectIterator<'a, T> {
        test_bounds(buf.len(), stride, x, y, w, h);
        MutableRectIterator {
            buf: &mut buf[(y * stride)..],
            stride,
            x0: x,
            x1: x + w,
            rows: h,
        }
    }

    pub fn from_rectangle(
        buf: &'a mut [T],
        stride: usize,
        r: &Rectangle,
    ) -> MutableRectIterator<'a, T> {
        MutableRectIterator::new(
            buf,
            stride,
            r.x as usize,
            r.y as usize,
            r.w as usize,
            r.h as usize,
        )
    }

    pub fn area(&self) -> usize {
        self.rows * (self.x1 - self.x0)
    }
}

impl<'a, T> Iterator for MutableRectIterator<'a, T> {
    type Item = &'a mut [T];
    fn next(&mut self) -> Option<Self::Item> {
        if self.rows > 0 {
            self.rows -= 1;
            let tmp = std::mem::take(&mut self.buf);
            let (slice, rest) = tmp.split_at_mut(self.stride);
            self.buf = rest;
            Some(&mut slice[self.x0..self.x1])
        } else {
            None
        }
    }

    fn size_hint(&self) -> (usize, Option<usize>) {
        (self.rows, Some(self.rows))
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use itertools::zip_eq;

    const BUFFER: [i32; 25] = [
        0, 1, 2, 3, 4, 10, 11, 12, 13, 14, 20, 21, 22, 23, 24, 30, 31, 32, 33, 34, 40, 41, 42, 43,
        44,
    ];

    #[test]
    fn test_full() {
        let mut crop = RectIterator::new(&BUFFER, 5, 0, 0, 5, 5);
        assert_eq!(crop.area(), 25);

        assert_eq!(crop.next(), Some(&BUFFER[0..5]));
        assert_eq!(crop.size_hint(), (4, Some(4)));
        assert_eq!(crop.next(), Some(&BUFFER[5..10]));
        assert_eq!(crop.next(), Some(&BUFFER[10..15]));
        assert_eq!(crop.next(), Some(&BUFFER[15..20]));
        assert_eq!(crop.next(), Some(&BUFFER[20..25]));
        assert_eq!(crop.next(), None);
    }

    #[test]
    #[should_panic]
    fn test_out_of_bounds() {
        let iter = RectIterator::new(&BUFFER, 5, 3, 0, 5, 5);
        for _ in iter {}
    }

    #[test]
    fn test_partial() {
        let expected = [[0, 1, 2], [10, 11, 12]];
        let crop = RectIterator::new(&BUFFER, 5, 0, 0, 3, 2);
        for (cropped, expected_row) in zip_eq(crop, expected.iter()) {
            assert_eq!(cropped, expected_row);
        }
    }

    #[test]
    fn test_partial_midle() {
        let expected = [[11, 12, 13], [21, 22, 23]];
        let crop = RectIterator::new(&BUFFER, 5, 1, 1, 3, 2);
        for (cropped, expected_row) in zip_eq(crop, expected.iter()) {
            assert_eq!(cropped, expected_row);
        }
    }

    #[test]
    fn test_partial_right() {
        let expected = [
            [1, 2, 3, 4],
            [11, 12, 13, 14],
            [21, 22, 23, 24],
            [31, 32, 33, 34],
            [41, 42, 43, 44],
        ];
        let crop = RectIterator::new(&BUFFER, 5, 1, 0, 4, 5);
        for (cropped, expected_row) in zip_eq(crop, expected.iter()) {
            assert_eq!(cropped, expected_row);
        }
    }

    #[test]
    fn test_partial_bottom() {
        let expected = [
            [10, 11, 12, 13, 14],
            [20, 21, 22, 23, 24],
            [30, 31, 32, 33, 34],
            [40, 41, 42, 43, 44],
        ];
        let crop = RectIterator::new(&BUFFER, 5, 0, 1, 5, 4);
        for (cropped, expected_row) in zip_eq(crop, expected.iter()) {
            assert_eq!(cropped, expected_row);
        }
    }

    #[test]
    fn test_mutation() {
        let mut buf = [0, 1, 2, 10, 11, 12, 20, 21, 22];
        let crop = MutableRectIterator::new(&mut buf, 3, 0, 0, 3, 3);
        assert_eq!(crop.area(), 9);
        assert_eq!(crop.size_hint(), (3, Some(3)));
        for pixel in crop.flatten() {
            *pixel = 0 - *pixel;
        }
        let expected = [0, -1, -2, -10, -11, -12, -20, -21, -22];
        assert_eq!(buf, expected);
    }

    #[test]
    fn test_mutation_partial() {
        let mut buf = [0, 1, 2, 3, 10, 11, 12, 12, 20, 21, 22, 13, 30, 31, 32, 33];
        let crop = MutableRectIterator::new(&mut buf, 4, 1, 1, 2, 2);
        for pixel in crop.flatten() {
            *pixel += 50;
        }
        let expected = [0, 1, 2, 3, 10, 61, 62, 12, 20, 71, 72, 13, 30, 31, 32, 33];
        assert_eq!(buf, expected);
    }

    #[test]
    fn test_mutation_partial_right() {
        let mut buf = [0, 1, 2, 3, 10, 11, 12, 12, 20, 21, 22, 13, 30, 31, 32, 33];
        let crop = MutableRectIterator::new(&mut buf, 4, 1, 0, 3, 4);
        for pixel in crop.flatten() {
            *pixel = 99;
        }
        let expected = [
            0, 99, 99, 99, 10, 99, 99, 99, 20, 99, 99, 99, 30, 99, 99, 99,
        ];
        assert_eq!(buf, expected);
    }
}
