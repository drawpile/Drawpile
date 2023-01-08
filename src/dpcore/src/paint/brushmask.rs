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

use super::color::{BIT15_F32, BIT15_U16};
use super::rect::Rectangle;
use super::rectiter::RectIterator;
use std::iter::FromIterator;

pub struct BrushMask {
    /// Brush diameter (mask shape is a square)
    pub diameter: u32,

    /// Brush mask (length is diameter^2)
    pub mask: Vec<u16>,
}

impl BrushMask {
    pub fn rect_iter(&self, r: &Rectangle) -> RectIterator<u16> {
        RectIterator::from_rectangle(&self.mask, self.diameter as usize, r)
    }
}

pub struct ClassicBrushCache {
    lut: Vec<Vec<f32>>,
}

impl ClassicBrushCache {
    pub fn new() -> ClassicBrushCache {
        ClassicBrushCache {
            lut: vec![Vec::new(); 101],
        }
    }

    fn get_cached_lut(&mut self, hardness: f32) -> &[f32] {
        let h = (hardness * 100.0) as usize;
        if self.lut[h].is_empty() {
            self.lut[h] = make_gimp_style_v2_brush_lut(hardness);
        }
        &self.lut[h]
    }
}

impl Default for ClassicBrushCache {
    fn default() -> Self {
        Self::new()
    }
}

fn square(v: f32) -> f32 {
    v * v
}

fn fast_sqrt(x: f32) -> f32 {
    let mut i = x.to_bits();
    i = i.wrapping_sub(1 << 23);
    i >>= 1;
    i = i.wrapping_add(1 << 29);
    f32::from_bits(i)
}

fn calculate_rr(
    xp: usize,
    yp: usize,
    x: f32,
    y: f32,
    aspect_ratio: f32,
    sn: f32,
    cs: f32,
    one_over_radius2: f32,
) -> f32 {
    let yy = (yp as f32) + 0.5f32 - y;
    let xx = (xp as f32) + 0.5f32 - x;
    let yyr = (yy * cs - xx * sn) * aspect_ratio;
    let xxr = yy * sn + xx * cs;
    (yyr * yyr + xxr * xxr) * one_over_radius2
}

fn closest_point_to_line(lx: f32, ly: f32, px: f32, py: f32) -> (f32, f32) {
    let l2 = lx * lx + ly * ly;
    let ltp_dot = px * lx + py * ly;
    let t = ltp_dot / l2;
    (lx * t, ly * t)
}

fn calculate_r_sample(x: f32, y: f32, aspect_ratio: f32, sn: f32, cs: f32) -> f32 {
    let yyr = (y * cs - x * sn) * aspect_ratio;
    let xxr = y * sn + x * cs;
    yyr * yyr + xxr * xxr
}

fn sign_point_in_line(px: f32, py: f32, vx: f32, vy: f32) -> f32 {
    (px - vx) * -vy - vx * (py - vy)
}

fn calculate_rr_antialiased(
    xp: usize,
    yp: usize,
    x: f32,
    y: f32,
    aspect_ratio: f32,
    sn: f32,
    cs: f32,
    one_over_radius2: f32,
    r_aa_start: f32,
) -> f32 {
    let pixel_right = x - (xp as f32);
    let pixel_bottom = y - (yp as f32);
    let pixel_center_x = pixel_right - 0.5f32;
    let pixel_center_y = pixel_bottom - 0.5f32;
    let pixel_left = pixel_right - 1.0f32;
    let pixel_top = pixel_bottom - 1.0f32;

    let (nearest_x, nearest_y, rr_near) = if pixel_left < 0.0f32
        && pixel_right > 0.0f32
        && pixel_top < 0.0f32
        && pixel_bottom > 0.0f32
    {
        (0.0f32, 0.0f32, 0.0f32)
    } else {
        let (nx, ny) = closest_point_to_line(cs, sn, pixel_center_x, pixel_center_y);
        let nxc = nx.clamp(pixel_left, pixel_right);
        let nyc = ny.clamp(pixel_top, pixel_bottom);
        let r_near = calculate_r_sample(nxc, nyc, aspect_ratio, sn, cs);
        (nxc, nyc, r_near * one_over_radius2)
    };

    if rr_near > 1.0f32 {
        rr_near
    } else {
        let center_sign = sign_point_in_line(pixel_center_x, pixel_center_y, cs, -sn);
        let rad_area_1: f32 = std::f32::consts::FRAC_1_PI.sqrt();

        let (farthest_x, farthest_y) = if center_sign < 0.0f32 {
            (nearest_x - sn * rad_area_1, nearest_y + cs * rad_area_1)
        } else {
            (nearest_x + sn * rad_area_1, nearest_y - cs * rad_area_1)
        };

        let r_far = calculate_r_sample(farthest_x, farthest_y, aspect_ratio, sn, cs);
        let rr_far = r_far * one_over_radius2;

        if r_far < r_aa_start {
            (rr_far + rr_near) * 0.5f32
        } else {
            let delta = rr_far - rr_near;
            let delta2 = 1.0f32 + delta;
            let visibility_near = (1.0f32 - rr_near) / delta2;
            1.0f32 - visibility_near
        }
    }
}

fn calculate_opa(
    rr: f32,
    hardness: f32,
    segment1_offset: f32,
    segment1_slope: f32,
    segment2_offset: f32,
    segment2_slope: f32,
) -> f32 {
    if rr > 1.0f32 {
        0.0f32
    } else {
        let (fac, opa) = if rr <= hardness {
            (segment1_slope, segment1_offset)
        } else {
            (segment2_slope, segment2_offset)
        };
        opa + rr * fac
    }
}

impl BrushMask {
    pub fn new_round_pixel(diameter: u32) -> BrushMask {
        let radius = diameter as f32 / 2.0;
        let rr = square(radius);
        let offset = 0.5_f32;

        let mut mask = vec![0u16; (diameter * diameter) as usize];
        let mut i = 0;

        for y in 0..diameter {
            let yy = square(y as f32 - radius + offset);
            for x in 0..diameter {
                let xx = square(x as f32 - radius + offset);
                if (yy + xx) < rr {
                    mask[i] = BIT15_U16;
                }
                i += 1;
            }
        }
        BrushMask { diameter, mask }
    }

    pub fn new_square_pixel(diameter: u32) -> BrushMask {
        BrushMask {
            diameter,
            mask: vec![BIT15_U16; (diameter * diameter) as usize],
        }
    }

    pub fn new_gimp_style_v2(
        x: f32,
        y: f32,
        diameter: f32,
        hardness: f32,
        cache: &mut ClassicBrushCache,
    ) -> (i32, i32, BrushMask) {
        let mask = if diameter <= 3.0 {
            BrushMask::new_gimp_style_v2_oversampled(x, y, diameter, hardness, cache)
        } else {
            BrushMask::new_gimp_style_v2_simple(x, y, diameter, hardness, cache)
        };

        let r = diameter as i32 / 2 + 1;
        (x as i32 - r, y as i32 - r, mask)
    }

    fn new_gimp_style_v2_oversampled(
        x: f32,
        y: f32,
        diameter: f32,
        hardness: f32,
        cache: &mut ClassicBrushCache,
    ) -> BrushMask {
        let idia = diameter.ceil().max(1.0) as usize + 2;

        let mut mask = vec![0u16; idia * idia];

        let fx = x.floor();
        let fy = y.floor();
        let xfrac = x - fx;
        let yfrac = y - fy;

        let diameter_offset = (1.0 - diameter % 2.0) / 2.0;
        let radius = diameter + 0.5; // oversample
        let xoffset = 0.5 - xfrac * 2.0 - radius - diameter_offset;
        let yoffset = 0.5 - yfrac * 2.0 - radius - diameter_offset;

        const OPACITY_SCALE: f32 = BIT15_F32 / 4.0; // 4 samples

        let lut = cache.get_cached_lut(hardness);
        let lut_scale = LUT_RADIUS / radius;

        for y in 0..idia {
            let yy1 = square(y as f32 * 2.0 + yoffset);
            let yy2 = square(y as f32 * 2.0 + yoffset + 1.0);
            for x in 0..idia {
                let xx1 = square(x as f32 * 2.0 + xoffset);
                let xx2 = square(x as f32 * 2.0 + xoffset + 1.0);

                let value = [xx1 + yy1, xx2 + yy1, xx1 + yy2, xx2 + yy2].iter().fold(
                    0.0,
                    |acc, dist_squared| {
                        let dist = fast_sqrt(*dist_squared);
                        let dist_scaled = (dist * lut_scale) as usize;
                        acc + if dist_scaled < lut.len() {
                            let cover = (radius - dist).clamp(0.0, 1.0);
                            lut[dist_scaled] * cover
                        } else {
                            0.0
                        }
                    },
                ) * OPACITY_SCALE;

                mask[y * idia + x] = value as u16;
            }
        }

        BrushMask {
            diameter: idia as u32,
            mask,
        }
    }

    fn new_gimp_style_v2_simple(
        x: f32,
        y: f32,
        diameter: f32,
        hardness: f32,
        cache: &mut ClassicBrushCache,
    ) -> BrushMask {
        let idia = diameter.ceil().max(1.0) as usize + 2;

        let mut mask = vec![0u16; idia * idia];

        let fx = x.floor();
        let fy = y.floor();
        let xfrac = x - fx;
        let yfrac = y - fy;

        let diameter_offset = (1.0 - diameter % 2.0) / 2.0;
        let radius = diameter / 2.0 + 0.5;
        let xoffset = 0.5 - xfrac - radius - diameter_offset;
        let yoffset = 0.5 - yfrac - radius - diameter_offset;

        const OPACITY_SCALE: f32 = BIT15_F32;

        let lut = cache.get_cached_lut(hardness);
        let lut_scale = LUT_RADIUS / radius;

        for y in 0..idia {
            let yy = square(y as f32 + yoffset);
            for x in 0..idia {
                let xx = square(x as f32 + xoffset);
                let dist = fast_sqrt(xx + yy);

                let dist_scaled = (dist * lut_scale) as usize;
                let value = if dist_scaled < lut.len() {
                    let cover = (radius - dist).clamp(0.0, 1.0);
                    lut[dist_scaled] * cover * OPACITY_SCALE
                } else {
                    0.0
                };

                mask[y * idia + x] = value as u16;
            }
        }

        BrushMask {
            diameter: idia as u32,
            mask,
        }
    }

    pub fn new_mypaint_brush_mask(
        x: f32,
        y: f32,
        diameter: f32,
        hardness: f32,
        aspect_ratio: f32,
        angle: f32,
    ) -> (i32, i32, BrushMask) {
        let idia = diameter.ceil().max(1.0) as usize + 2;
        let mask_length = idia * idia;

        let segment1_offset = 1.0f32;
        let segment1_slope = -(1.0f32 / hardness - 1.0f32);
        let segment2_offset = hardness / (1.0f32 - hardness);
        let segment2_slope = -hardness / (1.0f32 - hardness);

        let angle_rad = angle / 360.0 * 2.0 * std::f32::consts::PI;
        let cs = angle_rad.cos();
        let sn = angle_rad.sin();

        let radius = diameter / 2.0f32;
        let r_fringe = radius + 1.0f32;
        let x0 = ((radius - r_fringe).floor() as usize).max(0);
        let y0 = ((radius - r_fringe).floor() as usize).max(0);
        let x1 = (radius + r_fringe).floor() as usize;
        let y1 = (radius + r_fringe).floor() as usize;
        let one_over_radius2 = 1.0f32 / (radius * radius);

        let mut rr_mask = vec![0f32; mask_length];
        if radius < 3.0f32 {
            const AA_BORDER: f32 = 1.0f32;
            let aa_start = (radius - AA_BORDER).max(0.0f32);
            let r_aa_start = aa_start * (aa_start / aspect_ratio);
            for yp in y0..=y1 {
                for xp in x0..=x1 {
                    let rr = calculate_rr_antialiased(
                        xp,
                        yp,
                        radius,
                        radius,
                        aspect_ratio,
                        sn,
                        cs,
                        one_over_radius2,
                        r_aa_start,
                    );
                    rr_mask[yp * idia + xp] = rr;
                }
            }
        } else {
            for yp in y0..=y1 {
                for xp in x0..=x1 {
                    let rr = calculate_rr(
                        xp,
                        yp,
                        radius,
                        radius,
                        aspect_ratio,
                        sn,
                        cs,
                        one_over_radius2,
                    );
                    rr_mask[yp * idia + xp] = rr;
                }
            }
        }

        let mut mask = vec![0u16; mask_length];
        for yp in y0..=y1 {
            for xp in x0..=x1 {
                let rr = rr_mask[yp * idia + xp];
                let opa = calculate_opa(
                    rr,
                    hardness,
                    segment1_offset,
                    segment1_slope,
                    segment2_offset,
                    segment2_slope,
                );
                let opa_ = (opa * BIT15_F32) as u16;
                mask[yp * idia + xp] = opa_;
            }
        }

        (
            (x - radius + 0.5f32) as i32,
            (y - radius + 0.5f32) as i32,
            BrushMask {
                diameter: idia as u32,
                mask,
            },
        )
    }

    #[cfg(debug_assertions)]
    pub fn to_ascii_art(&self) -> String {
        let mut art = String::new();
        for y in 0..self.diameter {
            for x in 0..self.diameter {
                art.push(if self.mask[(y * self.diameter + x) as usize] == 0 {
                    '.'
                } else {
                    'X'
                });
            }
            art.push('\n');
        }
        art
    }
}

const LUT_RADIUS: f32 = 128.0;

// Generate a lookup table for Gimp style exponential brush shape
// The value at r (where r is distance from brush center, scaled to LUT_RADIUS) is
// the opaqueness of the pixel.
fn make_gimp_style_v2_brush_lut(hardness: f32) -> Vec<f32> {
    let exponent = if (1.0 - hardness) < 0.000_000_4 {
        1_000_000.0f32
    } else {
        0.4 / (1.0 - hardness)
    };

    Vec::<f32>::from_iter(
        (0..LUT_RADIUS as usize).map(|i| 1.0 - (i as f32 / LUT_RADIUS).powf(exponent)),
    )
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_round_pixel() {
        let mask = BrushMask::new_round_pixel(4);
        let expected: [u16; 4 * 4] = [
            0, BIT15_U16, BIT15_U16, 0, BIT15_U16, BIT15_U16, BIT15_U16, BIT15_U16, BIT15_U16,
            BIT15_U16, BIT15_U16, BIT15_U16, 0, BIT15_U16, BIT15_U16, 0,
        ];
        assert_eq!(mask.mask, expected);
    }

    #[test]
    fn test_square_pixel() {
        let mask = BrushMask::new_square_pixel(2);
        let expected: [u16; 4] = [BIT15_U16; 4];
        assert_eq!(mask.mask, expected);
    }
}
