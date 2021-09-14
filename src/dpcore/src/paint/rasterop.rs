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

use super::color::*;
use super::Blendmode;

pub fn pixel_blend(base: &mut [Pixel], over: &[Pixel], opacity: u8, mode: Blendmode) {
    match mode {
        Blendmode::Normal => alpha_pixel_blend(base, over, opacity),
        Blendmode::Erase => alpha_pixel_erase(base, over, opacity),
        Blendmode::Multiply => pixel_composite(comp_op_multiply, base, over, opacity),
        Blendmode::Divide => pixel_composite(comp_op_divide, base, over, opacity),
        Blendmode::Darken => pixel_composite(comp_op_darken, base, over, opacity),
        Blendmode::Lighten => pixel_composite(comp_op_lighten, base, over, opacity),
        Blendmode::Dodge => pixel_composite(comp_op_dodge, base, over, opacity),
        Blendmode::Burn => pixel_composite(comp_op_burn, base, over, opacity),
        Blendmode::Add => pixel_composite(comp_op_add, base, over, opacity),
        Blendmode::Subtract => pixel_composite(comp_op_subtract, base, over, opacity),
        Blendmode::Recolor => pixel_composite(comp_op_recolor, base, over, opacity),
        Blendmode::Behind => alpha_pixel_under(base, over, opacity),
        Blendmode::ColorErase => pixel_color_erase(base, over, opacity),
        Blendmode::Screen => pixel_composite(comp_op_screen, base, over, opacity),
        Blendmode::Replace => pixel_replace(base, over, opacity),
    }
}

pub fn mask_blend(base: &mut [Pixel], color: Pixel, mask: &[u8], mode: Blendmode) {
    match mode {
        Blendmode::Normal => alpha_mask_blend(base, color, mask),
        Blendmode::Erase => alpha_mask_erase(base, mask),
        Blendmode::Multiply => mask_composite(comp_op_multiply, base, color, mask),
        Blendmode::Divide => mask_composite(comp_op_divide, base, color, mask),
        Blendmode::Darken => mask_composite(comp_op_darken, base, color, mask),
        Blendmode::Lighten => mask_composite(comp_op_lighten, base, color, mask),
        Blendmode::Dodge => mask_composite(comp_op_dodge, base, color, mask),
        Blendmode::Burn => mask_composite(comp_op_burn, base, color, mask),
        Blendmode::Add => mask_composite(comp_op_add, base, color, mask),
        Blendmode::Subtract => mask_composite(comp_op_subtract, base, color, mask),
        Blendmode::Recolor => mask_composite(comp_op_recolor, base, color, mask),
        Blendmode::Behind => alpha_mask_under(base, color, mask),
        Blendmode::Screen => mask_composite(comp_op_screen, base, color, mask),
        Blendmode::ColorErase => mask_color_erase(base, color, mask),
        m => panic!("TODO unimplemented mask blend mode {:?}", m),
    }
}

trait ScratchArray {
    fn into_work(self) -> [u32; 4];
    fn from_work(p: [u32; 4]) -> Self;
}

impl ScratchArray for Pixel {
    fn from_work(p: [u32; 4]) -> Self {
        [p[0] as u8, p[1] as u8, p[2] as u8, p[3] as u8]
    }
    fn into_work(self) -> [u32; 4] {
        [
            self[0] as u32,
            self[1] as u32,
            self[2] as u32,
            self[3] as u32,
        ]
    }
}

fn u8_mult(a: u32, b: u32) -> u32 {
    let c = a * b + 0x80;
    ((c >> 8) + c) >> 8
}

/// Perform a premultiplied alpha blend operation on a slice of 32 bit ARGB pixels
fn alpha_pixel_blend(base: &mut [Pixel], over: &[Pixel], opacity: u8) {
    let o = opacity as u32;

    for (dp, sp) in base.iter_mut().zip(over.iter()) {
        let bp = dp.into_work();
        let src = sp.into_work();
        let a_s = 255 - u8_mult(src[ALPHA_CHANNEL], o);

        let result = [
            u8_mult(src[0], o) + u8_mult(bp[0], a_s),
            u8_mult(src[1], o) + u8_mult(bp[1], a_s),
            u8_mult(src[2], o) + u8_mult(bp[2], a_s),
            u8_mult(src[3], o) + u8_mult(bp[3], a_s),
        ];

        *dp = Pixel::from_work(result);
    }
}

/// Perform a premultiplied alpha blend operation on a slice of 32 bit ARGB pixels
fn alpha_pixel_under(base: &mut [Pixel], over: &[Pixel], opacity: u8) {
    let o = opacity as u32;

    for (dp, sp) in base.iter_mut().zip(over.iter()) {
        let bp = dp.into_work();
        let src = sp.into_work();
        let a_s = u8_mult(255 - bp[ALPHA_CHANNEL], u8_mult(src[ALPHA_CHANNEL], o));

        let result = [
            u8_mult(src[0], a_s) + bp[0],
            u8_mult(src[1], a_s) + bp[1],
            u8_mult(src[2], a_s) + bp[2],
            u8_mult(src[3], a_s) + bp[3],
        ];

        *dp = Pixel::from_work(result);
    }
}

/// Perform a premultiplied alpha blend on a slice of 32 bit ARGB pixels
/// and a color + alpha mask vector.
fn alpha_mask_under(base: &mut [Pixel], color: Pixel, mask: &[u8]) {
    debug_assert!(base.len() == mask.len());
    let c = color.into_work();

    for (dp, &mask) in base.iter_mut().zip(mask.iter()) {
        let bp = dp.into_work();
        let m = mask as u32;
        let a = u8_mult(255 - bp[ALPHA_CHANNEL], m);

        let result = [
            bp[0] + u8_mult(c[0], a),
            bp[1] + u8_mult(c[1], a),
            bp[2] + u8_mult(c[2], a),
            bp[3] + a,
        ];

        *dp = Pixel::from_work(result);
    }
}

fn color_erase(dest: &mut Color, color: &Color) {
    // This algorithm was found from the Gimp's source code:
    // for each channel:
    // a_c = (dest_c - color_c) / (1 - color_c) if dest_c > color_c
    //     (color_c - dest_c) / (color_c) if dest_c < color_c
    //     dest_c if color_c = 0
    //     0 if dest_c = color_c
    // a_a = dest_a
    //
    // dest_a = (1-color_a) + (max(a_r, a_g, a_b) * color_a)
    // dest_c = (dest_c - color_c) / dest_a + color_r
    // dest_a *= a_a

    fn ac(d: f32, c: f32) -> f32 {
        if c < (1.0 / 256.0) {
            d
        } else if d > c {
            (d - c) / (1.0 - c)
        } else if d < c {
            (c - d) / c
        } else {
            0.0
        }
    }

    let a = Color {
        r: ac(dest.r, color.r),
        g: ac(dest.g, color.g),
        b: ac(dest.b, color.b),
        a: dest.a,
    };

    dest.a = (1.0 - color.a) + a.r.max(a.b).max(a.g) * color.a;
    dest.r = (dest.r - color.r) / dest.a + color.r;
    dest.g = (dest.g - color.g) / dest.a + color.g;
    dest.b = (dest.b - color.b) / dest.a + color.b;
    dest.a *= a.a;
}

/// Perform per pixel color erase
fn pixel_color_erase(base: &mut [Pixel], over: &[Pixel], opacity: u8) {
    let o = opacity as f32 / 255.0;

    for (dp, sp) in base.iter_mut().zip(over.iter()) {
        // TODO optimize this?
        let mut dc = Color::from_pixel(*dp);
        let mut sc = Color::from_pixel(*sp);
        sc.a *= o;
        color_erase(&mut dc, &sc);
        *dp = dc.as_pixel();
    }
}

fn mask_color_erase(base: &mut [Pixel], color: Pixel, mask: &[u8]) {
    let mut c = Color::from_pixel(color);
    for (dp, &mp) in base.iter_mut().zip(mask.iter()) {
        // TODO optimize this?
        let mut dc = Color::from_pixel(*dp);
        c.a = mp as f32 / 255.0;
        color_erase(&mut dc, &c);
        *dp = dc.as_pixel();
    }
}

fn pixel_replace(base: &mut [Pixel], over: &[Pixel], opacity: u8) {
    let o = opacity as u32;

    for (dp, sp) in base.iter_mut().zip(over.iter()) {
        let src = sp.into_work();
        *dp = [
            u8_mult(src[0], o) as u8,
            u8_mult(src[1], o) as u8,
            u8_mult(src[2], o) as u8,
            u8_mult(src[3], o) as u8,
        ];
    }
}

/// Perform a premultiplied alpha blend on a slice of 32 bit ARGB pixels
/// and a color + alpha mask vector.
fn alpha_mask_blend(base: &mut [Pixel], color: Pixel, mask: &[u8]) {
    debug_assert!(base.len() == mask.len());
    let c = color.into_work();

    for (dp, &mask) in base.iter_mut().zip(mask.iter()) {
        let bp = dp.into_work();
        let m = mask as u32;
        let a = 255 - m;

        let result = [
            u8_mult(c[0], m) + u8_mult(bp[0], a),
            u8_mult(c[1], m) + u8_mult(bp[1], a),
            u8_mult(c[2], m) + u8_mult(bp[2], a),
            m + u8_mult(bp[3], a),
        ];

        *dp = Pixel::from_work(result);
    }
}

/// Erase alpha channel
fn alpha_mask_erase(base: &mut [Pixel], mask: &[u8]) {
    debug_assert!(base.len() == mask.len());

    for (dp, &mask) in base.iter_mut().zip(mask.iter()) {
        let mut dest = dp.into_work();
        let m = mask as u32;
        let a = 255 - m;

        for d in dest.iter_mut() {
            *d = u8_mult(*d, a);
        }
        *dp = Pixel::from_work(dest);
    }
}

/// Erase using alpha channel of source pixels
fn alpha_pixel_erase(base: &mut [Pixel], over: &[Pixel], opacity: u8) {
    let o = opacity as u32;

    for (dp, sp) in base.iter_mut().zip(over.iter()) {
        let a = 255 - u8_mult(sp[ALPHA_CHANNEL] as u32, o);
        let bp = dp.into_work();
        let result = [
            u8_mult(bp[0], a),
            u8_mult(bp[1], a),
            u8_mult(bp[2], a),
            u8_mult(bp[3], a),
        ];
        *dp = Pixel::from_work(result);
    }
}

fn blend(a: f32, b: f32, alpha: f32) -> f32 {
    (a - b) * alpha + b
}

fn comp_op_multiply(a: f32, b: f32) -> f32 {
    a * b
}

fn comp_op_screen(a: f32, b: f32) -> f32 {
    1.0f32 - (1.0f32 - a) * (1.0f32 - b)
}

fn comp_op_divide(a: f32, b: f32) -> f32 {
    1.0f32.min(a / ((1.0 / 256.0) + b))
}

fn comp_op_darken(a: f32, b: f32) -> f32 {
    a.min(b)
}

fn comp_op_lighten(a: f32, b: f32) -> f32 {
    a.max(b)
}

fn comp_op_dodge(a: f32, b: f32) -> f32 {
    1.0f32.min(a / (1.001 - b))
}

fn comp_op_burn(a: f32, b: f32) -> f32 {
    0.0f32.max(1.0f32.min(1.0 - ((1.0 - a) / (b + 0.001))))
}

fn comp_op_add(a: f32, b: f32) -> f32 {
    1.0f32.min(a + b)
}

fn comp_op_subtract(a: f32, b: f32) -> f32 {
    0.0f32.max(a - b)
}

fn comp_op_recolor(_: f32, b: f32) -> f32 {
    b
}

/// Generic alpha-preserving compositing operations
fn pixel_composite(comp_op: fn(f32, f32) -> f32, base: &mut [Pixel], over: &[Pixel], opacity: u8) {
    let of = opacity as f32 / 255.0;
    for (dp, sp) in base.iter_mut().zip(over.iter()) {
        // TODO optimize this. These operations need non-premultiplied color
        let mut dc = Color::from_pixel(*dp);
        let sc = Color::from_pixel(*sp);

        let alpha = sc.a * of;

        dc.r = blend(comp_op(dc.r, sc.r), dc.r, alpha);
        dc.g = blend(comp_op(dc.g, sc.g), dc.g, alpha);
        dc.b = blend(comp_op(dc.b, sc.b), dc.b, alpha);

        *dp = dc.as_pixel();
    }
}

fn mask_composite(comp_op: fn(f32, f32) -> f32, base: &mut [Pixel], color: Pixel, mask: &[u8]) {
    debug_assert!(base.len() == mask.len());
    let c = Color::from_pixel(color);
    for (dp, &mask) in base.iter_mut().zip(mask.iter()) {
        let mut d = Color::from_pixel(*dp);
        let m = mask as f32 / 255.0;

        d.r = blend(comp_op(d.r, c.r), d.r, m);
        d.g = blend(comp_op(d.g, c.g), d.g, m);
        d.b = blend(comp_op(d.b, c.b), d.b, m);

        *dp = d.as_pixel();
    }
}

pub fn tint_pixels(pixels: &mut [Pixel], tint: Color) {
    for px in pixels {
        // TODO optimize this. This operation works on non-premultiplied color
        let mut p = Color::from_pixel(*px);
        p.r = blend(tint.r, p.r, tint.a);
        p.g = blend(tint.g, p.g, tint.a);
        p.b = blend(tint.b, p.b, tint.a);

        *px = p.as_pixel();
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_alpha_pixel_blend() {
        let mut base = [[255, 0, 0, 255]];
        let over = [[0, 128, 0, 128]];

        alpha_pixel_blend(&mut base, &over, 0xff);
        assert_eq!(base, [[127, 128, 0, 255]]);

        let mut base = [[255, 0, 0, 255]];
        alpha_pixel_blend(&mut base, &over, 0x80);
        assert_eq!(base, [[191, 64, 0, 255]]);
    }

    #[test]
    fn test_alpha_mask_blend() {
        let mut base = [Color::rgb8(255, 0, 0).as_pixel(); 3];
        let mask = [0xff, 0x80, 0x40];

        alpha_mask_blend(&mut base, Color::rgb8(0, 0, 255).as_pixel(), &mask);
        assert_eq!(
            base,
            [
                Color::rgb8(0, 0, 255).as_pixel(),
                Color::rgb8(127, 0, 128).as_pixel(),
                Color::rgb8(191, 0, 64).as_pixel(),
            ]
        );
    }

    #[test]
    fn test_alpha_pixel_erase() {
        let mut base = [
            [255, 255, 255, 255],
            [255, 255, 255, 255],
            [255, 255, 255, 255],
        ];
        let over = [[1, 2, 3, 255], [1, 2, 3, 128], [1, 2, 3, 0]];

        alpha_pixel_erase(&mut base, &over, 0xff);
        assert_eq!(
            base,
            [[0, 0, 0, 0], [127, 127, 127, 127], [255, 255, 255, 255]]
        );
    }
    #[test]
    fn test_alpha_mask_erase() {
        let mut base = [
            [255, 255, 255, 255],
            [255, 255, 255, 255],
            [255, 255, 255, 255],
        ];
        let mask = [0xff, 0x80, 0x00];

        alpha_mask_erase(&mut base, &mask);
        assert_eq!(
            base,
            [[0, 0, 0, 0], [127, 127, 127, 127], [255, 255, 255, 255]]
        );
    }
}
