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

use super::color::{Color, Pixel, ALPHA_CHANNEL};
use super::tile::{Tile, TILE_SIZE};
use super::{Image, InternalLayerID, Layer, LayerID, LayerStack, Rectangle};

use std::sync::Arc;

pub struct FloodFillResult {
    pub image: Image,
    pub x: i32,
    pub y: i32,
    pub layer_seed_color: Color,
    pub fill_color: Color,
    pub oversize: bool,
}

impl FloodFillResult {
    fn empty() -> FloodFillResult {
        FloodFillResult {
            image: Image::default(),
            x: 0,
            y: 0,
            layer_seed_color: Color::TRANSPARENT,
            fill_color: Color::TRANSPARENT,
            oversize: false,
        }
    }
}

pub fn floodfill(
    image: &LayerStack,
    x: i32,
    y: i32,
    fill_color: Color,
    tolerance: f32,
    layer_id: LayerID,
    sample_merged: bool,
    size_limit: u32,
) -> FloodFillResult {
    let layer = match image.get_layer(layer_id) {
        Some(l) => l,
        None => {
            return FloodFillResult::empty();
        }
    };

    if x < 0 || y < 0 || x as u32 >= image.width() || y as u32 >= image.height() {
        return FloodFillResult::empty();
    }

    // The scratch layer is used for two things: for flattening the layerstack in sample_merged mode
    // and to keep track of filled pixels.
    let mut scratch = ScratchLayer {
        layer: Layer::new(
            InternalLayerID(0),
            layer.width(),
            layer.height(),
            Tile::Blank,
        ),
        fill_layer: Layer::new(
            InternalLayerID(0),
            layer.width(),
            layer.height(),
            Tile::Blank,
        ),
        source_layer: layer,
        image,
        sample_merged,
        reference_color: Color::TRANSPARENT,
        tolerance_squared: tolerance * tolerance,
        fill_pixel: fill_color.as_pixel(),
    };

    // First, get the color at the starting coordinates.
    let layer_seed_color = Color::from_pixel(layer.pixel_at(x as u32, y as u32));
    scratch.reference_color = Color::from_pixel(scratch.pixel_at(x, y));

    // If the fill color is transparent (i.e. we're doing a flood erase)
    // we have to pick some target color other than the reference color.
    // (For erase mode, we only care about the result alpha channel)
    if fill_color.is_transparent() {
        if scratch.reference_color.is_transparent() {
            // Already transparent: nothing to do
            return FloodFillResult::empty();
        }

        let c = if scratch.reference_color.r < 0.01 {
            Color::WHITE
        } else {
            Color::BLACK
        };
        return floodfill(
            image,
            x,
            y,
            c,
            tolerance,
            layer_id,
            sample_merged,
            size_limit,
        );
    }

    // Execute the flood fill algorithm
    let mut stack = vec![(x as i32, y as i32)];
    let mut filled_size = 0;

    let right = scratch.layer.width() as i32 - 1;

    while !stack.is_empty() && filled_size < size_limit {
        let (x, mut y) = stack.pop().unwrap();

        let mut span_left = false;
        let mut span_right = false;

        while y >= 0 && scratch.is_ref_color_at(x, y) {
            y -= 1;
        }
        y += 1;

        while y < scratch.layer.height() as i32 && scratch.is_ref_color_at(x, y) {
            scratch.set_pixel(x as u32, y as u32);
            filled_size += 1;

            if !span_left && x > 0 && scratch.is_ref_color_at(x - 1, y) {
                stack.push((x - 1, y));
                span_left = true;
            } else if span_left && x > 0 && !scratch.is_ref_color_at(x - 1, y) {
                span_left = false;
            } else if !span_right && x < right && scratch.is_ref_color_at(x + 1, y) {
                stack.push((x + 1, y));
                span_right = true;
            } else if span_right && x < right && !scratch.is_ref_color_at(x + 1, y) {
                span_right = false;
            }
            y += 1;
        }
    }

    // Return the results
    let (result_image, x, y) = scratch.fill_layer.to_cropped_image();

    FloodFillResult {
        image: result_image,
        x,
        y,
        layer_seed_color,
        fill_color,
        oversize: filled_size >= size_limit,
    }
}

struct ScratchLayer<'a> {
    /// this is the actual scratch layer
    layer: Layer,
    /// filled pixels are put here too. This becomes the final result
    fill_layer: Layer,
    /// the target/source layer
    source_layer: &'a Layer,
    /// the layer stack is used in sample_merged mode
    image: &'a LayerStack,
    /// in sample merged mode, the entire layerstack is flattened into the scratch layer
    sample_merged: bool,
    /// the color to be filled over
    reference_color: Color,
    /// max allowed color distance (squared)
    tolerance_squared: f32,
    /// the pixel value to fill with
    fill_pixel: Pixel,
}

impl<'a> ScratchLayer<'a> {
    fn pixel_at(&mut self, x: i32, y: i32) -> Pixel {
        let x = x as u32;
        let y = y as u32;
        // If this tile doesn't exist yet in the scratch layer,
        // copy it from the source layer or flatten from the whole stack
        let tx = x / TILE_SIZE;
        let ty = y / TILE_SIZE;
        if *self.layer.tile(tx, ty) == Tile::Blank {
            self.layer.replace_tile(
                tx,
                ty,
                if self.sample_merged {
                    Tile::Bitmap(Arc::new(self.image.flatten_tile(tx, ty)))
                } else {
                    self.source_layer.tile(tx, ty).clone()
                },
            );
        }

        return self.layer.pixel_at(x, y);
    }

    fn is_ref_color_at(&mut self, x: i32, y: i32) -> bool {
        let c = Color::from_pixel(self.pixel_at(x, y));
        // TODO better color distance function
        let r = c.r - self.reference_color.r;
        let g = c.g - self.reference_color.g;
        let b = c.b - self.reference_color.b;
        let a = c.a - self.reference_color.a;

        r * r + g * g + b * b + a * a <= self.tolerance_squared
    }

    fn set_pixel(&mut self, x: u32, y: u32) {
        let ti = x / TILE_SIZE;
        let tj = y / TILE_SIZE;
        let tx = x - ti * TILE_SIZE;
        let ty = y - tj * TILE_SIZE;
        self.layer
            .tile_mut(ti, tj)
            .set_pixel_at(0, tx, ty, self.fill_pixel);
        self.fill_layer
            .tile_mut(ti, tj)
            .set_pixel_at(0, tx, ty, self.fill_pixel);
    }
}

pub fn expand_floodfill(input: FloodFillResult, expansion: i32) -> FloodFillResult {
    if input.image.is_null() || expansion < 1 {
        return input;
    }

    // Step 1. Make sure there is enough room for expansion
    // There should be at least *dia* pixels around the content rectangle
    // to accommodate the convolution kernel
    let content_bounds = match input.image.opaque_bounds() {
        Some(b) => b,
        None => {
            return input;
        }
    };

    let image_area = Rectangle::new(0, 0, input.image.width as i32, input.image.height as i32);
    let dia = expansion * 2 + 1;

    let padded_area = Rectangle::new(
        content_bounds.x - dia,
        content_bounds.y - dia,
        content_bounds.w + dia * 2,
        content_bounds.h + dia * 2,
    );
    let (image, xoffset, yoffset, expansion_area) = if image_area.contains(&padded_area) {
        // There is enough room
        (
            input.image,
            input.x,
            input.y,
            Rectangle::new(
                content_bounds.x - expansion,
                content_bounds.y - expansion,
                content_bounds.w + expansion * 2,
                content_bounds.h + expansion * 2,
            ),
        )
    } else {
        // Not enough room! Image must be padded.
        let mut new_image = Image::new(padded_area.w as usize, padded_area.h as usize);
        input
            .image
            .rect_iter(&content_bounds)
            .zip(new_image.rect_iter_mut(&Rectangle::new(
                dia,
                dia,
                content_bounds.w,
                content_bounds.h,
            )))
            .for_each(|(src, dest)| dest.copy_from_slice(src));
        (
            new_image,
            input.x + padded_area.x,
            input.y + padded_area.y,
            Rectangle::new(
                expansion,
                expansion,
                content_bounds.w + expansion * 2,
                content_bounds.h + expansion * 2,
            ),
        )
    };

    // Step 2. Generate the convolution kernel
    let kernel = {
        let mut kernel = vec![false; (dia * dia) as usize];
        let rr = expansion * expansion;
        for y in 0..dia {
            let y0 = y - expansion;
            for x in 0..dia {
                let x0 = x - expansion;
                kernel[(y * dia + x) as usize] = (y0 * y0 + x0 * x0) <= rr;
            }
        }
        kernel
    };

    // Step 3. Apply the kernel to produce an expanded image
    let mut new_image = Image::new(image.width, image.height);

    let fill_pixel = input.fill_color.as_pixel();
    for y in expansion_area.y..expansion_area.bottom() {
        for x in expansion_area.x..expansion_area.right() {
            'outer: for ky in 0..dia {
                for kx in 0..dia {
                    if kernel[(ky * dia + kx) as usize]
                        && image.pixels[(y + ky - expansion) as usize * image.width
                            + (x + kx - expansion) as usize][ALPHA_CHANNEL]
                            > 0
                    {
                        new_image.pixels[y as usize * image.width + x as usize] = fill_pixel;
                        break 'outer;
                    }
                }
            }
        }
    }

    // Step 4. Crop negative parts.
    // The protocol doesn't support negative coordinates and this part would
    // get cropped anyway. We could also crop any part that goes over the
    // right/bottom edge, but we don't know the canvas size here.
    if xoffset < 0 || yoffset < 0 {
        let xshift = 0.max(-xoffset);
        let yshift = 0.max(-yoffset);
        let crop_rect = Rectangle::new(
            xshift,
            yshift,
            new_image.width as i32 - xshift,
            new_image.height as i32 - yshift,
        );

        let mut crop_image = Image::new(crop_rect.w as usize, crop_rect.h as usize);
        new_image
            .rect_iter(&crop_rect)
            .zip(crop_image.rect_iter_mut(&Rectangle::new(0, 0, crop_rect.w, crop_rect.h)))
            .for_each(|(src, dest)| dest.copy_from_slice(src));

        FloodFillResult {
            image: crop_image,
            x: xoffset + xshift,
            y: yoffset + yshift,
            ..input
        }
    } else {
        FloodFillResult {
            image: new_image,
            x: xoffset,
            y: yoffset,
            ..input
        }
    }
}
