// This file is part of Drawpile.
// Copyright (C) 2020-2022 Calle Laakkonen, askmeaboutloom
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

pub fn pixel_blend(base: &mut [Pixel15], over: &[Pixel15], opacity: u16, mode: Blendmode) {
    let o = opacity as u32;
    #[allow(unreachable_patterns)]
    match mode {
        // Alpha-affecting blend modes
        Blendmode::Normal => pixel_normal(base, over, o),
        Blendmode::Behind => pixel_behind(base, over, o),
        Blendmode::Erase => pixel_erase(base, over, o),
        Blendmode::ColorErase => pixel_color_erase(base, over, o),
        Blendmode::Replace => pixel_replace(base, over, o),
        // Alpha-preserving separable blend modes (each channel handled separately)
        Blendmode::Multiply => pixel_composite(comp_op_multiply, base, over, o),
        Blendmode::Divide => pixel_composite(comp_op_divide, base, over, o),
        Blendmode::Darken => pixel_composite(comp_op_darken, base, over, o),
        Blendmode::Lighten => pixel_composite(comp_op_lighten, base, over, o),
        Blendmode::Dodge => pixel_composite(comp_op_dodge, base, over, o),
        Blendmode::Burn => pixel_composite(comp_op_burn, base, over, o),
        Blendmode::Add => pixel_composite(comp_op_add, base, over, o),
        Blendmode::Subtract => pixel_composite(comp_op_subtract, base, over, o),
        Blendmode::Recolor => pixel_composite(comp_op_recolor, base, over, o),
        Blendmode::Screen => pixel_composite(comp_op_screen, base, over, o),
        Blendmode::Overlay => pixel_composite(comp_op_overlay, base, over, o),
        Blendmode::HardLight => pixel_composite(comp_op_hard_light, base, over, o),
        Blendmode::SoftLight => pixel_composite(comp_op_soft_light, base, over, o),
        Blendmode::LinearBurn => pixel_composite(comp_op_linear_burn, base, over, o),
        Blendmode::LinearLight => pixel_composite(comp_op_linear_light, base, over, o),
        // Alpha-preserving separable blend modes where the opacity affects blending
        Blendmode::LuminosityShineSai => {
            pixel_composite_with_opacity(comp_op_luminosity_shine_sai, base, over, o)
        }
        // Alpha-preserving non-separable blend modes (channels interact)
        Blendmode::Hue => pixel_composite_nonseparable(comp_op_hue, base, over, o),
        Blendmode::Saturation => pixel_composite_nonseparable(comp_op_sat, base, over, o),
        Blendmode::Luminosity => pixel_composite_nonseparable(comp_op_lum, base, over, o),
        Blendmode::Color => pixel_composite_nonseparable(comp_op_color, base, over, o),
        // Unknown blend mode, don't do anything.
        _m => {
            #[cfg(debug_assertions)]
            warn!("Unknown pixel blend mode {:?}", _m);
        }
    }
}

pub fn mask_blend(
    base: &mut [Pixel15],
    color: Pixel15,
    mask: &[u16],
    mode: Blendmode,
    opacity: u16,
) {
    debug_assert!(base.len() == mask.len());
    let o = opacity as u32;
    #[allow(unreachable_patterns)]
    match mode {
        // Alpha-affecting blend modes
        Blendmode::Normal => mask_normal(base, color, mask, o),
        Blendmode::NormalAndEraser => mask_normal_and_erase(base, color, mask, o),
        Blendmode::Behind => mask_behind(base, color, mask, o),
        Blendmode::Erase => mask_erase(base, mask, o),
        Blendmode::ColorErase => mask_color_erase(base, color, mask, o),
        Blendmode::Replace => mask_replace(base, color, mask, o),
        // Alpha-preserving separable blend modes (each channel handled separately)
        Blendmode::Multiply => mask_composite(comp_op_multiply, base, color, mask, o),
        Blendmode::Divide => mask_composite(comp_op_divide, base, color, mask, o),
        Blendmode::Darken => mask_composite(comp_op_darken, base, color, mask, o),
        Blendmode::Lighten => mask_composite(comp_op_lighten, base, color, mask, o),
        Blendmode::Dodge => mask_composite(comp_op_dodge, base, color, mask, o),
        Blendmode::Burn => mask_composite(comp_op_burn, base, color, mask, o),
        Blendmode::Add => mask_composite(comp_op_add, base, color, mask, o),
        Blendmode::Subtract => mask_composite(comp_op_subtract, base, color, mask, o),
        Blendmode::Recolor => mask_composite(comp_op_recolor, base, color, mask, o),
        Blendmode::Screen => mask_composite(comp_op_screen, base, color, mask, o),
        Blendmode::Overlay => mask_composite(comp_op_overlay, base, color, mask, o),
        Blendmode::HardLight => mask_composite(comp_op_hard_light, base, color, mask, o),
        Blendmode::SoftLight => mask_composite(comp_op_soft_light, base, color, mask, o),
        Blendmode::LinearBurn => mask_composite(comp_op_linear_burn, base, color, mask, o),
        Blendmode::LinearLight => mask_composite(comp_op_linear_light, base, color, mask, o),
        // Alpha-preserving separable blend modes where the opacity affects blending
        Blendmode::LuminosityShineSai => {
            mask_composite_with_opacity(comp_op_luminosity_shine_sai, base, color, mask, o)
        }
        // Alpha-preserving non-separable blend modes (channels interact)
        Blendmode::Hue => mask_composite_nonseparable(comp_op_hue, base, color, mask, o),
        Blendmode::Saturation => mask_composite_nonseparable(comp_op_sat, base, color, mask, o),
        Blendmode::Luminosity => mask_composite_nonseparable(comp_op_lum, base, color, mask, o),
        Blendmode::Color => mask_composite_nonseparable(comp_op_color, base, color, mask, o),
        // Unknown blend mode, don't do anything.
        _m => {
            #[cfg(debug_assertions)]
            warn!("Unknown mask blend mode {:?}", _m);
        }
    }
}

// For working with the 15 bit types, we need to expand them to a u32.
type BGRA15 = [u32; 4];
type BGR15 = [u32; 3];

fn unpack_pixel15(p: Pixel15) -> BGRA15 {
    [p[0] as u32, p[1] as u32, p[2] as u32, p[3] as u32]
}

fn unpack_unpremultiply_pixel15(p: Pixel15) -> BGRA15 {
    unpack_pixel15(unpremultiply_pixel15(p))
}

fn unpack_bgr(p: Pixel15) -> BGR15 {
    [p[0] as u32, p[1] as u32, p[2] as u32]
}

fn unpack_unpremultiply_bgr(p: Pixel15) -> BGR15 {
    unpack_bgr(unpremultiply_pixel15(p))
}

// 15 bit math, adapted from MyPaint; Copyright (C) 2012 by Andrew Chadwick,
// licensed under GPL version 2 or later.

fn u15_mul(a: u32, b: u32) -> u32 {
    (a * b) >> 15
}

fn u15_sumprods(a1: u32, a2: u32, b1: u32, b2: u32) -> u32 {
    ((a1 * a2) + (b1 * b2)) >> 15
}

// Babylonian method of calculating a square root. The lookup table is
// calculated via floor(((i / 16.0) ** 0.5) * (1<<16)) - 1 for i in 1..=17.

const U15_SQRT_LOOKUP: [u32; 16] = [
    16383, 23169, 28376, 32767, 36634, 40131, 43346, 46339, 49151, 51809, 54338, 56754, 59072,
    61302, 63453, 65535,
];

fn u15_sqrt(x: u32) -> u32 {
    if x == 0 || x == BIT15_U32 {
        x
    } else {
        let s = x << 1;
        let mut n = U15_SQRT_LOOKUP[(s >> 12) as usize];
        let mut n_old;
        for _ in 0..15 {
            n_old = n;
            n += (s << 16) / n;
            n >>= 1;
            if (n == n_old)
                || ((n > n_old) && (n - 1 == n_old))
                || ((n < n_old) && (n + 1 == n_old))
            {
                break;
            }
        }
        n >> 1
    }
}

fn lumu(bgr: BGR15) -> u32 {
    const LUM_R: u32 = (0.3 * BIT15_F32) as u32;
    const LUM_G: u32 = (0.59 * BIT15_F32) as u32;
    const LUM_B: u32 = (0.11 * BIT15_F32) as u32;
    (LUM_R * bgr[RED_CHANNEL] + LUM_G * bgr[GREEN_CHANNEL] + LUM_B * bgr[BLUE_CHANNEL]) / BIT15_U32
}

fn lumi(bgr: [i32; 3]) -> i32 {
    const LUM_R: i32 = (0.3 * BIT15_F32) as i32;
    const LUM_G: i32 = (0.59 * BIT15_F32) as i32;
    const LUM_B: i32 = (0.11 * BIT15_F32) as i32;
    (LUM_R * bgr[RED_CHANNEL] + LUM_G * bgr[GREEN_CHANNEL] + LUM_B * bgr[BLUE_CHANNEL]) / BIT15_I32
}

fn clip_color(bgr: [i32; 3]) -> BGR15 {
    let l = lumi(bgr);
    let n = bgr[0].min(bgr[1].min(bgr[2]));
    let x = bgr[0].max(bgr[1].max(bgr[2]));
    let mut out = bgr;
    if n < 0 {
        out[0] = l + (((out[0] - l) * l) / (l - n));
        out[1] = l + (((out[1] - l) * l) / (l - n));
        out[2] = l + (((out[2] - l) * l) / (l - n));
    }
    if x > BIT15_I32 {
        out[0] = l + (((out[0] - l) * (BIT15_I32 - l)) / (x - l));
        out[1] = l + (((out[1] - l) * (BIT15_I32 - l)) / (x - l));
        out[2] = l + (((out[2] - l) * (BIT15_I32 - l)) / (x - l));
    }
    [out[0] as u32, out[1] as u32, out[2] as u32]
}

fn set_lum(bgr: BGR15, l: u32) -> BGR15 {
    let d = l as i32 - lumu(bgr) as i32;
    clip_color([bgr[0] as i32 + d, bgr[1] as i32 + d, bgr[2] as i32 + d])
}

fn sat(bgr: BGR15) -> u32 {
    bgr[0].max(bgr[1].max(bgr[2])) - bgr[0].min(bgr[1].min(bgr[2]))
}

fn set_sat(bgr: BGR15, s: u32) -> BGR15 {
    let mut max = 0usize;
    let mut mid = 1usize;
    let mut min = 2usize;
    if bgr[max] < bgr[mid] {
        (max, mid) = (mid, max);
    }
    if bgr[max] < bgr[min] {
        (max, min) = (min, max);
    }
    if bgr[mid] < bgr[min] {
        (mid, min) = (min, mid);
    }

    let mut out = [0u32; 3];
    if bgr[max] > bgr[min] {
        out[mid] = ((bgr[mid] - bgr[min]) * s) / (bgr[max] - bgr[min]);
        out[max] = s;
    } else {
        out[mid] = 0;
        out[max] = 0;
    }
    out[min] = 0;
    out
}

// Pixel blend operations.

fn pixel_normal(base: &mut [Pixel15], over: &[Pixel15], o: u32) {
    for (dp, sp) in base.iter_mut().zip(over.iter()) {
        let a_s = BIT15_U32 - u15_mul(sp[ALPHA_CHANNEL] as u32, o);
        *dp = [
            (u15_mul(sp[0] as u32, o) + u15_mul(dp[0] as u32, a_s)) as u16,
            (u15_mul(sp[1] as u32, o) + u15_mul(dp[1] as u32, a_s)) as u16,
            (u15_mul(sp[2] as u32, o) + u15_mul(dp[2] as u32, a_s)) as u16,
            (u15_mul(sp[3] as u32, o) + u15_mul(dp[3] as u32, a_s)) as u16,
        ];
    }
}

fn mask_normal(base: &mut [Pixel15], color: Pixel15, mask: &[u16], o: u32) {
    let c = unpack_pixel15(color);
    for (dp, &m) in base.iter_mut().zip(mask.iter()) {
        let a = u15_mul(m as u32, o);
        let a_s = BIT15_U32 - a;
        *dp = [
            (u15_mul(c[0], a) + u15_mul(dp[0] as u32, a_s)) as u16,
            (u15_mul(c[1], a) + u15_mul(dp[1] as u32, a_s)) as u16,
            (u15_mul(c[2], a) + u15_mul(dp[2] as u32, a_s)) as u16,
            (a + u15_mul(dp[3] as u32, a_s)) as u16,
        ];
    }
}

// Like normal alpha blending, but taking the alpha of the color into account.
// See also MyPaint's draw_dab_pixels_BlendMode_Normal_and_Eraser.
fn mask_normal_and_erase(base: &mut [Pixel15], color: Pixel15, mask: &[u16], o: u32) {
    let u = unpack_unpremultiply_pixel15(color);
    for (dp, &m) in base.iter_mut().zip(mask.iter()) {
        let a = m as u32 * o / BIT15_U32;
        let opa_b = BIT15_U32 - a;
        let opa_a = a * u[3] / BIT15_U32;
        *dp = [
            ((opa_a * u[0] + opa_b * dp[0] as u32) / BIT15_U32) as u16,
            ((opa_a * u[1] + opa_b * dp[1] as u32) / BIT15_U32) as u16,
            ((opa_a * u[2] + opa_b * dp[2] as u32) / BIT15_U32) as u16,
            (opa_a + opa_b * dp[3] as u32 / BIT15_U32) as u16,
        ];
    }
}

fn pixel_behind(base: &mut [Pixel15], over: &[Pixel15], o: u32) {
    for (dp, sp) in base.iter_mut().zip(over.iter()) {
        let da = dp[ALPHA_CHANNEL] as u32;
        let sa = sp[ALPHA_CHANNEL] as u32;
        let a_s = u15_mul(BIT15_U32 - da, u15_mul(sa, o));
        *dp = [
            (u15_mul(sp[0] as u32, a_s) + dp[0] as u32) as u16,
            (u15_mul(sp[1] as u32, a_s) + dp[1] as u32) as u16,
            (u15_mul(sp[2] as u32, a_s) + dp[2] as u32) as u16,
            (u15_mul(sa, a_s) + da) as u16,
        ];
    }
}

fn mask_behind(base: &mut [Pixel15], color: Pixel15, mask: &[u16], o: u32) {
    let c = unpack_pixel15(color);
    for (dp, &m) in base.iter_mut().zip(mask.iter()) {
        let da = dp[ALPHA_CHANNEL] as u32;
        let a = u15_mul(m as u32, o);
        let a_s = u15_mul(BIT15_U32 - da, a);
        *dp = [
            (dp[0] as u32 + u15_mul(c[0], a_s)) as u16,
            (dp[1] as u32 + u15_mul(c[1], a_s)) as u16,
            (dp[2] as u32 + u15_mul(c[2], a_s)) as u16,
            (da + a_s) as u16,
        ];
    }
}

fn pixel_erase(base: &mut [Pixel15], over: &[Pixel15], o: u32) {
    for (dp, sp) in base.iter_mut().zip(over.iter()) {
        let a_s = BIT15_U32 - u15_mul(sp[ALPHA_CHANNEL] as u32, o);
        *dp = [
            u15_mul(dp[0] as u32, a_s) as u16,
            u15_mul(dp[1] as u32, a_s) as u16,
            u15_mul(dp[2] as u32, a_s) as u16,
            u15_mul(dp[3] as u32, a_s) as u16,
        ];
    }
}

fn mask_erase(base: &mut [Pixel15], mask: &[u16], o: u32) {
    for (dp, &m) in base.iter_mut().zip(mask.iter()) {
        let a_s = BIT15_U32 - u15_mul(m as u32, o);
        *dp = [
            u15_mul(dp[0] as u32, a_s) as u16,
            u15_mul(dp[1] as u32, a_s) as u16,
            u15_mul(dp[2] as u32, a_s) as u16,
            u15_mul(dp[3] as u32, a_s) as u16,
        ];
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
fn pixel_color_erase(base: &mut [Pixel15], over: &[Pixel15], o: u32) {
    let of = o as f32 / BIT15_F32;
    for (dp, sp) in base.iter_mut().zip(over.iter()) {
        // TODO optimize this?
        let mut dc = Color::from_pixel15(*dp);
        let mut sc = Color::from_pixel15(*sp);
        sc.a *= of;
        color_erase(&mut dc, &sc);
        *dp = dc.as_pixel15();
    }
}

fn mask_color_erase(base: &mut [Pixel15], color: Pixel15, mask: &[u16], o: u32) {
    let mut c = Color::from_pixel15(color);
    for (dp, &m) in base.iter_mut().zip(mask.iter()) {
        // TODO optimize this?
        let mut dc = Color::from_pixel15(*dp);
        c.a = u15_mul(m as u32, o) as f32 / BIT15_F32;
        color_erase(&mut dc, &c);
        *dp = dc.as_pixel15();
    }
}

fn pixel_replace(base: &mut [Pixel15], over: &[Pixel15], o: u32) {
    for (dp, sp) in base.iter_mut().zip(over.iter()) {
        *dp = [
            u15_mul(sp[0] as u32, o) as u16,
            u15_mul(sp[1] as u32, o) as u16,
            u15_mul(sp[2] as u32, o) as u16,
            u15_mul(sp[3] as u32, o) as u16,
        ];
    }
}

fn mask_replace(base: &mut [Pixel15], color: Pixel15, mask: &[u16], o: u32) {
    let c = unpack_pixel15(color);
    for (dp, &m) in base.iter_mut().zip(mask.iter()) {
        let a = u15_mul(m as u32, o);
        *dp = [
            u15_mul(c[0], a) as u16,
            u15_mul(c[1], a) as u16,
            u15_mul(c[2], a) as u16,
            a as u16,
        ];
    }
}

// Separable compositing functions (ones that can be applied channel by channel)

fn comp_op_multiply(a: u32, b: u32) -> u32 {
    u15_mul(a, b)
}

fn comp_op_screen(a: u32, b: u32) -> u32 {
    BIT15_U32 - u15_mul(BIT15_U32 - a, BIT15_U32 - b)
}

fn comp_op_overlay(a: u32, b: u32) -> u32 {
    comp_op_hard_light(b, a)
}

fn comp_op_hard_light(a: u32, b: u32) -> u32 {
    let b2 = b * 2;
    if b2 <= BIT15_U32 {
        comp_op_multiply(a, b2)
    } else {
        comp_op_screen(a, b2 - BIT15_U32)
    }
}

fn comp_op_soft_light(a: u32, b: u32) -> u32 {
    let b2 = b * 2;
    if b2 <= BIT15_U32 {
        a - u15_mul(u15_mul(BIT15_U32 - b2, a), BIT15_U32 - a)
    } else {
        let a4 = a * 4;
        let d = if a4 <= BIT15_U32 {
            let squared = u15_mul(a, a);
            a4 + 16 * u15_mul(squared, a) - 12 * squared
        } else {
            u15_sqrt(a)
        };
        a + u15_mul(b2 - BIT15_U32, d - a)
    }
}

fn comp_op_divide(a: u32, b: u32) -> u32 {
    BIT15_U32.min((a * (BIT15_U32 + 1) + b / 2) / (1 + b))
}

fn comp_op_darken(a: u32, b: u32) -> u32 {
    a.min(b)
}

fn comp_op_lighten(a: u32, b: u32) -> u32 {
    a.max(b)
}

fn comp_op_dodge(a: u32, b: u32) -> u32 {
    BIT15_U32.min(a * (BIT15_U32 + 1) / ((BIT15_U32 + 1) - b))
}

fn comp_op_burn(a: u32, b: u32) -> u32 {
    (BIT15_I32 - ((BIT15_U32 - a) * (BIT15_U32 + 1) / (b + 1)) as i32).clamp(0, BIT15_I32) as u32
}

fn comp_op_add(a: u32, b: u32) -> u32 {
    BIT15_U32.min(a + b)
}

fn comp_op_subtract(a: u32, b: u32) -> u32 {
    0.max(a as i32 - b as i32) as u32
}

fn comp_op_linear_burn(a: u32, b: u32) -> u32 {
    BIT15_U32.max(a + b) - BIT15_U32
}

fn comp_op_linear_light(a: u32, b: u32) -> u32 {
    ((a + 2 * b) as i32 - BIT15_I32).clamp(0, BIT15_I32) as u32
}

fn comp_op_recolor(_: u32, b: u32) -> u32 {
    b
}

// This blend mode is from Paint Tool SAI, version 1 calls it "Luminosity",
// version 2 calls it "Shine". Krita calls it "Luminosity/Shine (SAI)", so we
// do too. It works by Normal blending the new pixel with a fully opaque black
// pixel and then compositing the result via Addition.
fn comp_op_luminosity_shine_sai(a: u32, b: u32, o: u32) -> u32 {
    comp_op_add(a, u15_mul(b, o))
}

fn comp_op_hue(dc: BGR15, sc: BGR15) -> BGR15 {
    set_lum(set_sat(sc, sat(dc)), lumu(dc))
}

fn comp_op_sat(dc: BGR15, sc: BGR15) -> BGR15 {
    set_lum(set_sat(dc, sat(sc)), lumu(dc))
}

fn comp_op_lum(dc: BGR15, sc: BGR15) -> BGR15 {
    set_lum(dc, lumu(sc))
}

fn comp_op_color(dc: BGR15, sc: BGR15) -> BGR15 {
    set_lum(sc, lumu(dc))
}

// Generic alpha-preserving compositing operations

fn pixel_composite(comp_op: fn(u32, u32) -> u32, base: &mut [Pixel15], over: &[Pixel15], o: u32) {
    for (dp, sp) in base.iter_mut().zip(over.iter()) {
        let dc = unpack_unpremultiply_bgr(*dp);
        let sc = unpack_unpremultiply_bgr(*sp);
        let a = u15_mul(sp[ALPHA_CHANNEL] as u32, o);
        let a_s = BIT15_U32 - a;
        *dp = premultiply_pixel15([
            u15_sumprods(a_s, dc[0], a, comp_op(dc[0], sc[0])) as u16,
            u15_sumprods(a_s, dc[1], a, comp_op(dc[1], sc[1])) as u16,
            u15_sumprods(a_s, dc[2], a, comp_op(dc[2], sc[2])) as u16,
            dp[3],
        ]);
    }
}

fn mask_composite(
    comp_op: fn(u32, u32) -> u32,
    base: &mut [Pixel15],
    color: Pixel15,
    mask: &[u16],
    o: u32,
) {
    let u = unpack_unpremultiply_bgr(color);
    for (dp, &m) in base.iter_mut().zip(mask.iter()) {
        let dc = unpack_unpremultiply_bgr(*dp);
        let a = u15_mul(m as u32, o);
        let a_s = BIT15_U32 - a;
        *dp = premultiply_pixel15([
            u15_sumprods(a_s, dc[0], a, comp_op(dc[0], u[0])) as u16,
            u15_sumprods(a_s, dc[1], a, comp_op(dc[1], u[1])) as u16,
            u15_sumprods(a_s, dc[2], a, comp_op(dc[2], u[2])) as u16,
            dp[3],
        ]);
    }
}

// Same as the above, just that the opacity is passed into the composition

fn pixel_composite_with_opacity(
    comp_op: fn(u32, u32, u32) -> u32,
    base: &mut [Pixel15],
    over: &[Pixel15],
    o: u32,
) {
    for (dp, sp) in base.iter_mut().zip(over.iter()) {
        let dc = unpack_unpremultiply_bgr(*dp);
        let sc = unpack_unpremultiply_bgr(*sp);
        let a = u15_mul(sp[ALPHA_CHANNEL] as u32, o);
        let a_s = BIT15_U32 - a;
        *dp = premultiply_pixel15([
            u15_sumprods(a_s, dc[0], a, comp_op(dc[0], sc[0], a)) as u16,
            u15_sumprods(a_s, dc[1], a, comp_op(dc[1], sc[1], a)) as u16,
            u15_sumprods(a_s, dc[2], a, comp_op(dc[2], sc[2], a)) as u16,
            dp[3],
        ]);
    }
}

fn mask_composite_with_opacity(
    comp_op: fn(u32, u32, u32) -> u32,
    base: &mut [Pixel15],
    color: Pixel15,
    mask: &[u16],
    o: u32,
) {
    let u = unpack_unpremultiply_bgr(color);
    for (dp, &m) in base.iter_mut().zip(mask.iter()) {
        let dc = unpack_unpremultiply_bgr(*dp);
        let a = u15_mul(m as u32, o);
        let a_s = BIT15_U32 - a;
        *dp = premultiply_pixel15([
            u15_sumprods(a_s, dc[0], a, comp_op(dc[0], u[0], a)) as u16,
            u15_sumprods(a_s, dc[1], a, comp_op(dc[1], u[1], a)) as u16,
            u15_sumprods(a_s, dc[2], a, comp_op(dc[2], u[2], a)) as u16,
            dp[3],
        ]);
    }
}

fn pixel_composite_nonseparable(
    comp_op: fn(BGR15, BGR15) -> BGR15,
    base: &mut [Pixel15],
    over: &[Pixel15],
    o: u32,
) {
    for (dp, sp) in base.iter_mut().zip(over.iter()) {
        let dc = unpack_unpremultiply_bgr(*dp);
        let rc = comp_op(dc, unpack_unpremultiply_bgr(*sp));
        let a = u15_mul(sp[ALPHA_CHANNEL] as u32, o);
        let a_s = BIT15_U32 - a;
        *dp = premultiply_pixel15([
            u15_sumprods(a_s, dc[0], a, rc[0]) as u16,
            u15_sumprods(a_s, dc[1], a, rc[1]) as u16,
            u15_sumprods(a_s, dc[2], a, rc[2]) as u16,
            dp[3],
        ]);
    }
}

fn mask_composite_nonseparable(
    comp_op: fn(BGR15, BGR15) -> BGR15,
    base: &mut [Pixel15],
    color: Pixel15,
    mask: &[u16],
    o: u32,
) {
    let u = unpack_unpremultiply_bgr(color);
    for (dp, &m) in base.iter_mut().zip(mask.iter()) {
        let dc = unpack_unpremultiply_bgr(*dp);
        let rc = comp_op(dc, u);
        let a = u15_mul(m as u32, o);
        let a_s = BIT15_U32 - a;
        *dp = premultiply_pixel15([
            u15_sumprods(a_s, dc[0], a, rc[0]) as u16,
            u15_sumprods(a_s, dc[1], a, rc[1]) as u16,
            u15_sumprods(a_s, dc[2], a, rc[2]) as u16,
            dp[3],
        ]);
    }
}

// Effectively uses the Recolor blend mode to tint the given pixels.
pub fn tint_pixels(pixels: &mut [Pixel15], tint: Color) {
    let tint = unpack_pixel15(tint.as_unpremultiplied_pixel15());
    let a = tint[ALPHA_CHANNEL];
    for dp in pixels {
        let dc = unpack_unpremultiply_bgr(*dp);
        let a_s = BIT15_U32 - a;
        *dp = premultiply_pixel15([
            u15_sumprods(a_s, dc[0], a, tint[0]) as u16,
            u15_sumprods(a_s, dc[1], a, tint[1]) as u16,
            u15_sumprods(a_s, dc[2], a, tint[2]) as u16,
            dp[3],
        ]);
    }
}
