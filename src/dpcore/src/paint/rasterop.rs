// This file is part of Drawpile.
// Copyright (C) 2020-2021 Calle Laakkonen
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
#[cfg(debug_assertions)]
use tracing::warn;

pub fn pixel_blend(base: &mut [Pixel], over: &[Pixel], opacity: u8, mode: Blendmode) {
    #[allow(unreachable_patterns)]
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
        Blendmode::LuminosityShineSai => pixel_luminosity_shine_sai(base, over, opacity),
        Blendmode::Overlay => pixel_composite(comp_op_overlay, base, over, opacity),
        Blendmode::HardLight => pixel_composite(comp_op_hard_light, base, over, opacity),
        Blendmode::SoftLight => pixel_composite(comp_op_soft_light, base, over, opacity),
        Blendmode::Replace => pixel_replace(base, over, opacity),
        _m => {
            #[cfg(debug_assertions)]
            warn!("Unknown pixel blend mode {:?}", _m);
        }
    }
}

pub fn mask_blend(base: &mut [Pixel], color: Pixel, mask: &[u8], mode: Blendmode, opacity: u8) {
    let o = opacity as u32;
    #[allow(unreachable_patterns)]
    match mode {
        Blendmode::Normal => alpha_mask_blend(base, color, mask, o),
        Blendmode::Erase => alpha_mask_erase(base, mask, o),
        Blendmode::Multiply => mask_composite(comp_op_multiply, base, color, mask, o),
        Blendmode::Divide => mask_composite(comp_op_divide, base, color, mask, o),
        Blendmode::Darken => mask_composite(comp_op_darken, base, color, mask, o),
        Blendmode::Lighten => mask_composite(comp_op_lighten, base, color, mask, o),
        Blendmode::Dodge => mask_composite(comp_op_dodge, base, color, mask, o),
        Blendmode::Burn => mask_composite(comp_op_burn, base, color, mask, o),
        Blendmode::Add => mask_composite(comp_op_add, base, color, mask, o),
        Blendmode::Subtract => mask_composite(comp_op_subtract, base, color, mask, o),
        Blendmode::Recolor => mask_composite(comp_op_recolor, base, color, mask, o),
        Blendmode::Behind => alpha_mask_under(base, color, mask, o),
        Blendmode::Screen => mask_composite(comp_op_screen, base, color, mask, o),
        Blendmode::ColorErase => mask_color_erase(base, color, mask, o),
        Blendmode::NormalAndEraser => alpha_mask_blend_erase(base, color, mask, o),
        Blendmode::LuminosityShineSai => mask_luminosity_shine_sai(base, color, mask, o),
        Blendmode::Overlay => mask_composite(comp_op_overlay, base, color, mask, o),
        Blendmode::HardLight => mask_composite(comp_op_hard_light, base, color, mask, o),
        Blendmode::SoftLight => mask_composite(comp_op_soft_light, base, color, mask, o),
        _m => {
            #[cfg(debug_assertions)]
            warn!("Unknown mask blend mode {:?}", _m);
        }
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

fn u8_blend(a: i32, b: i32, alpha: i32) -> i32 {
    let c = (a - b) * alpha + (b << 8) - b + 0x80;
    ((c >> 8) + c) >> 8
}

// 8 bit square root lookup table. The values are floored, so the math is
// floor(sqrt(i / 255.0) * 255.0) for i in 0 .. 255.
const U8_SQRT_LOOKUP: [u32; 256] = [
    0, 15, 22, 27, 31, 35, 39, 42, 45, 47, 50, 52, 55, 57, 59, 61, 63, 65, 67, 69, 71, 73, 74, 76,
    78, 79, 81, 82, 84, 85, 87, 88, 90, 91, 93, 94, 95, 97, 98, 99, 100, 102, 103, 104, 105, 107,
    108, 109, 110, 111, 112, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127,
    128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 141, 142, 143, 144, 145,
    146, 147, 148, 148, 149, 150, 151, 152, 153, 153, 154, 155, 156, 157, 158, 158, 159, 160, 161,
    162, 162, 163, 164, 165, 165, 166, 167, 168, 168, 169, 170, 171, 171, 172, 173, 174, 174, 175,
    176, 177, 177, 178, 179, 179, 180, 181, 182, 182, 183, 184, 184, 185, 186, 186, 187, 188, 188,
    189, 190, 190, 191, 192, 192, 193, 194, 194, 195, 196, 196, 197, 198, 198, 199, 200, 200, 201,
    201, 202, 203, 203, 204, 205, 205, 206, 206, 207, 208, 208, 209, 210, 210, 211, 211, 212, 213,
    213, 214, 214, 215, 216, 216, 217, 217, 218, 218, 219, 220, 220, 221, 221, 222, 222, 223, 224,
    224, 225, 225, 226, 226, 227, 228, 228, 229, 229, 230, 230, 231, 231, 232, 233, 233, 234, 234,
    235, 235, 236, 236, 237, 237, 238, 238, 239, 240, 240, 241, 241, 242, 242, 243, 243, 244, 244,
    245, 245, 246, 246, 247, 247, 248, 248, 249, 249, 250, 250, 251, 251, 252, 252, 253, 253, 254,
    255,
];

fn u8_sqrt(a: u32) -> u32 {
    U8_SQRT_LOOKUP[a as usize]
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
fn alpha_mask_under(base: &mut [Pixel], color: Pixel, mask: &[u8], opacity: u32) {
    debug_assert!(base.len() == mask.len());
    let c = color.into_work();

    for (dp, &mask) in base.iter_mut().zip(mask.iter()) {
        let bp = dp.into_work();
        let m = u8_mult(mask as u32, opacity);
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

fn mask_color_erase(base: &mut [Pixel], color: Pixel, mask: &[u8], opacity: u32) {
    let mut c = Color::from_pixel(color);
    for (dp, &mp) in base.iter_mut().zip(mask.iter()) {
        // TODO optimize this?
        let mut dc = Color::from_pixel(*dp);
        c.a = u8_mult(mp as u32, opacity) as f32 / 255.0;
        color_erase(&mut dc, &c);
        *dp = dc.as_pixel();
    }
}

// This blend mode is from Paint Tool SAI, version 1 calls it "Luminosity",
// version 2 calls it "Shine". Krita calls it "Luminosity/Shine (SAI)", so we
// do too. It works by Normal blending the new pixel with a fully opaque black
// pixel and then compositing the result via Addition.

fn pixel_luminosity_shine_sai(base: &mut [Pixel], over: &[Pixel], opacity: u8) {
    let o = opacity as u32;
    for (dp, sp) in base.iter_mut().zip(over.iter()) {
        *dp = apply_luminosity_shine(*dp, [sp[0] as u32, sp[1] as u32, sp[2] as u32], o);
    }
}

fn mask_luminosity_shine_sai(base: &mut [Pixel], color: Pixel, mask: &[u8], opacity: u32) {
    debug_assert!(base.len() == mask.len());
    let c = [color[0] as u32, color[1] as u32, color[2] as u32];
    for (dp, &mask) in base.iter_mut().zip(mask.iter()) {
        *dp = apply_luminosity_shine(*dp, c, u8_mult(mask as u32, opacity));
    }
}

fn apply_luminosity_shine(dp: Pixel, sp: [u32; 3], o: u32) -> Pixel {
    let dc = unpremultiply_pixel(dp);
    premultiply_pixel([
        u8_blend(
            comp_op_add(dc[0] as u32, u8_mult(sp[0], o)) as i32,
            dc[0] as i32,
            o as i32,
        ) as u8,
        u8_blend(
            comp_op_add(dc[1] as u32, u8_mult(sp[1], o)) as i32,
            dc[1] as i32,
            o as i32,
        ) as u8,
        u8_blend(
            comp_op_add(dc[2] as u32, u8_mult(sp[2], o)) as i32,
            dc[2] as i32,
            o as i32,
        ) as u8,
        dc[3],
    ])
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
fn alpha_mask_blend(base: &mut [Pixel], color: Pixel, mask: &[u8], opacity: u32) {
    debug_assert!(base.len() == mask.len());
    let c = color.into_work();

    for (dp, &mask) in base.iter_mut().zip(mask.iter()) {
        let bp = dp.into_work();
        let m = u8_mult(mask as u32, opacity);
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
fn alpha_mask_erase(base: &mut [Pixel], mask: &[u8], opacity: u32) {
    debug_assert!(base.len() == mask.len());

    for (dp, &mask) in base.iter_mut().zip(mask.iter()) {
        let mut dest = dp.into_work();
        let m = u8_mult(mask as u32, opacity);
        let a = 255 - m;

        for d in dest.iter_mut() {
            *d = u8_mult(*d, a);
        }
        *dp = Pixel::from_work(dest);
    }
}

// Like normal alpha blending, but taking the alpha of the color into account.
// See also MyPaint's draw_dab_pixels_BlendMode_Normal_and_Eraser.
fn alpha_mask_blend_erase(base: &mut [Pixel], color: Pixel, mask: &[u8], opacity: u32) {
    debug_assert!(base.len() == mask.len());
    let c = unpremultiply_pixel(color).into_work();

    for (dp, &mask) in base.iter_mut().zip(mask.iter()) {
        let bp = dp.into_work();
        let m = u8_mult(mask as u32, opacity);
        let a = 255 - m;
        let n = m * c[3] / 256; // Take the color alpha into account.

        let result = [
            u8_mult(c[0], n) + u8_mult(bp[0], a),
            u8_mult(c[1], n) + u8_mult(bp[1], a),
            u8_mult(c[2], n) + u8_mult(bp[2], a),
            n + u8_mult(bp[3], a),
        ];

        *dp = Pixel::from_work(result);
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

fn comp_op_multiply(a: u32, b: u32) -> u32 {
    u8_mult(a, b)
}

fn comp_op_screen(a: u32, b: u32) -> u32 {
    255 - u8_mult(255 - a, 255 - b)
}

fn comp_op_overlay(a: u32, b: u32) -> u32 {
    comp_op_hard_light(b, a)
}

fn comp_op_hard_light(a: u32, b: u32) -> u32 {
    let b2 = b * 2;
    if b2 <= 255 {
        comp_op_multiply(a, b2)
    } else {
        comp_op_screen(a, b2 - 255)
    }
}

fn comp_op_soft_light(a: u32, b: u32) -> u32 {
    let b2 = b * 2;
    if b2 <= 255 {
        a - u8_mult(u8_mult(255 - b2, a), 255 - a)
    } else {
        let a4 = a * 4;
        let d = if a4 <= 255 {
            let squared = u8_mult(a, a);
            a4 + 16 * u8_mult(squared, a) - 12 * squared
        } else {
            u8_sqrt(a)
        };
        a + u8_mult(b2 - 255, d - a)
    }
}

fn comp_op_divide(a: u32, b: u32) -> u32 {
    255.min((a * 256 + b / 2) / (1 + b))
}

fn comp_op_darken(a: u32, b: u32) -> u32 {
    a.min(b)
}

fn comp_op_lighten(a: u32, b: u32) -> u32 {
    a.max(b)
}

fn comp_op_dodge(a: u32, b: u32) -> u32 {
    255.min(a * 256 / (256 - b))
}

fn comp_op_burn(a: u32, b: u32) -> u32 {
    (255 - ((255 - a) * 256 / (b + 1)) as i32).clamp(0, 255) as u32
}

fn comp_op_add(a: u32, b: u32) -> u32 {
    255.min(a + b)
}

fn comp_op_subtract(a: u32, b: u32) -> u32 {
    0.max(a as i32 - b as i32) as u32
}

fn comp_op_recolor(_: u32, b: u32) -> u32 {
    b
}

/// Generic alpha-preserving compositing operations
fn pixel_composite(comp_op: fn(u32, u32) -> u32, base: &mut [Pixel], over: &[Pixel], opacity: u8) {
    for (dp, sp) in base.iter_mut().zip(over.iter()) {
        let dc = unpremultiply_pixel(*dp);
        let sc = unpremultiply_pixel(*sp);

        let alpha = u8_mult(sc[ALPHA_CHANNEL] as u32, opacity as u32) as i32;

        *dp = premultiply_pixel([
            u8_blend(
                comp_op(dc[0] as u32, sc[0] as u32) as i32,
                dc[0] as i32,
                alpha,
            ) as u8,
            u8_blend(
                comp_op(dc[1] as u32, sc[1] as u32) as i32,
                dc[1] as i32,
                alpha,
            ) as u8,
            u8_blend(
                comp_op(dc[2] as u32, sc[2] as u32) as i32,
                dc[2] as i32,
                alpha,
            ) as u8,
            dc[3],
        ]);
    }
}

fn mask_composite(
    comp_op: fn(u32, u32) -> u32,
    base: &mut [Pixel],
    color: Pixel,
    mask: &[u8],
    opacity: u32,
) {
    debug_assert!(base.len() == mask.len());
    let c = unpremultiply_pixel(color);

    for (dp, &mask) in base.iter_mut().zip(mask.iter()) {
        let d = unpremultiply_pixel(*dp);
        let mask = u8_mult(mask as u32, opacity) as i32;

        *dp = premultiply_pixel([
            u8_blend(comp_op(d[0] as u32, c[0] as u32) as i32, d[0] as i32, mask) as u8,
            u8_blend(comp_op(d[1] as u32, c[1] as u32) as i32, d[1] as i32, mask) as u8,
            u8_blend(comp_op(d[2] as u32, c[2] as u32) as i32, d[2] as i32, mask) as u8,
            d[3],
        ]);
    }
}

pub fn tint_pixels(pixels: &mut [Pixel], tint: Color) {
    let tint = tint.as_unpremultiplied_pixel();
    let a = tint[ALPHA_CHANNEL] as i32;
    for px in pixels {
        let p = unpremultiply_pixel(*px);
        *px = premultiply_pixel([
            u8_blend(tint[0] as i32, p[0] as i32, a) as u8,
            u8_blend(tint[1] as i32, p[1] as i32, a) as u8,
            u8_blend(tint[2] as i32, p[2] as i32, a) as u8,
            p[3],
        ]);
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

        alpha_mask_blend(&mut base, Color::rgb8(0, 0, 255).as_pixel(), &mask, 255);
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

        alpha_mask_erase(&mut base, &mask, 255);
        assert_eq!(
            base,
            [[0, 0, 0, 0], [127, 127, 127, 127], [255, 255, 255, 255]]
        );
    }
}
