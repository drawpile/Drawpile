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

use super::aoe::{AoE, TileMap};
use super::color::{ALPHA_CHANNEL, ZERO_PIXEL};
use super::rectiter::RectIterator;
use super::tile::{Tile, TILE_SIZE, TILE_SIZEI};
use super::{
    rasterop, BitmapLayer, Blendmode, BrushMask, Color, GroupLayer, Layer, LayerID, Pixel,
    Rectangle, UserID,
};

/// Fills a rectangle with a solid color using the given blending mode
///
/// # Arguments
///
/// * `layer` - The target layer
/// * `user` - User ID tag to attach to the changed tiles
/// * `color` - Fill color
/// * `mode` - Fill blending mode
/// * `rect` - Rectangle to fill
pub fn fill_rect(
    layer: &mut BitmapLayer,
    user: UserID,
    color: &Color,
    mode: Blendmode,
    rect: &Rectangle,
) -> AoE {
    let r = match rect.cropped(layer.size()) {
        Some(r) => r,
        None => return AoE::Nothing,
    };

    let pixels = [color.as_pixel(); TILE_SIZE as usize];
    let can_paint = mode.can_increase_opacity();
    let can_erase = mode.can_decrease_opacity();

    for (i, j, tile) in layer.tile_rect_mut(&r) {
        if !can_paint && *tile == Tile::Blank {
            continue;
        }

        let x = i * TILE_SIZEI;
        let y = j * TILE_SIZEI;
        let subrect = Rectangle::new(x, y, TILE_SIZEI, TILE_SIZEI)
            .intersected(&r)
            .unwrap()
            .offset(-x, -y);
        for row in tile.rect_iter_mut(user, &subrect, can_erase) {
            rasterop::pixel_blend(row, &pixels, 0xff, mode);
        }
    }

    r.into()
}

/// Clear a layer
pub fn clear_layer(layer: &mut BitmapLayer) -> AoE {
    let old_content = layer.nonblank_tilemap();
    layer
        .tilevec_mut()
        .iter_mut()
        .for_each(|t| *t = Tile::Blank);
    old_content.into()
}

/// Draw a brush mask onto the layer
///
/// Note: When drawing in "indirect" mode, the brush dabs should be
/// drawn to to a sublayer. The sublayer is then merged at the end of the stroke.
///
/// # Arguments
///
/// * `layer` - The target layer
/// * `user` - User ID tag to attach to the changed tiles
/// * `x` - Left edge of the mask
/// * `y` - Top edge of the mask
/// * `mask` - The brush mask
/// * `color` - The brush color
/// * `mode` - Brush blending mode
pub fn draw_brush_dab(
    layer: &mut BitmapLayer,
    user: UserID,
    x: i32,
    y: i32,
    mask: &BrushMask,
    color: &Color,
    mode: Blendmode,
) -> AoE {
    let d = mask.diameter as i32;
    let rect = match Rectangle::new(x, y, d, d).cropped(layer.size()) {
        Some(r) => r,
        None => return AoE::Nothing,
    };

    let colorpix = color.as_pixel();
    let can_paint = mode.can_increase_opacity();
    let can_erase = mode.can_decrease_opacity();

    for (i, j, tile) in layer.tile_rect_mut(&rect) {
        if !can_paint && *tile == Tile::Blank {
            continue;
        }

        let x0 = i * TILE_SIZEI;
        let y0 = j * TILE_SIZEI;
        let subrect = Rectangle::new(x0, y0, TILE_SIZEI, TILE_SIZEI)
            .intersected(&rect)
            .unwrap();
        let tilerect = subrect.offset(-x0, -y0);
        let maskrect = rect.intersected(&subrect).unwrap().offset(-x, -y);

        for (destrow, maskrow) in
            tile.rect_iter_mut(user, &tilerect, can_erase)
                .zip(RectIterator::from_rectangle(
                    &mask.mask,
                    mask.diameter as usize,
                    &maskrect,
                ))
        {
            rasterop::mask_blend(destrow, colorpix, maskrow, mode);
        }
    }

    rect.into()
}

/// Draw an image onto the layer
///
/// The given image must be in premultiplied ARGB format.
/// The image will be drawn onto the given rectangle. The width and height
/// of the rectangle must match the image dimensions. The rectangle may be
/// outside the layer boundaries; it will be cropped as needed.
pub fn draw_image(
    layer: &mut BitmapLayer,
    user: UserID,
    image: &[Pixel],
    rect: &Rectangle,
    opacity: f32,
    blendmode: Blendmode,
) -> AoE {
    assert_eq!(image.len(), (rect.w * rect.h) as usize);
    let destrect = match rect.cropped(layer.size()) {
        Some(r) => r,
        None => return AoE::Nothing,
    };

    let o = (opacity * 255.0) as u8;
    let can_paint = blendmode.can_increase_opacity();
    let can_erase = blendmode.can_decrease_opacity();

    for (i, j, tile) in layer.tile_rect_mut(&destrect) {
        if !can_paint && *tile == Tile::Blank {
            continue;
        }

        let x0 = i * TILE_SIZEI;
        let y0 = j * TILE_SIZEI;
        let subrect = Rectangle::new(x0, y0, TILE_SIZEI, TILE_SIZEI)
            .intersected(&destrect)
            .unwrap();
        let tilerect = subrect.offset(-x0, -y0);
        let srcrect = rect.intersected(&subrect).unwrap().offset(-rect.x, -rect.y);

        for (destrow, imagerow) in
            tile.rect_iter_mut(user, &tilerect, can_erase)
                .zip(RectIterator::from_rectangle(
                    image,
                    rect.w as usize,
                    &srcrect,
                ))
        {
            rasterop::pixel_blend(destrow, imagerow, o, blendmode);
        }
    }

    destrect.into()
}

/// Replace a tile or a stretch of tiles.
/// This is typically used to set the initial canvas content
/// at the start of a session.
pub fn put_tile(
    layer: &mut BitmapLayer,
    sublayer: LayerID,
    col: u32,
    row: u32,
    repeat: u32,
    tile: &Tile,
) -> AoE {
    if sublayer != 0 {
        return put_tile(
            layer.get_or_create_sublayer(sublayer),
            0,
            col,
            row,
            repeat,
            tile,
        );
    }

    let xtiles = Tile::div_up(layer.width());
    let tilevec = layer.tilevec_mut();
    if tilevec.is_empty() {
        return AoE::Nothing;
    }

    let start = (row * xtiles + col) as usize;

    if start >= tilevec.len() {
        return AoE::Nothing;
    }

    let end = (tilevec.len() - 1).min(start + repeat as usize);
    for t in &mut tilevec[start..=end] {
        *t = tile.clone();
    }

    if end > start {
        let mut aoe = TileMap::new(layer.width(), layer.height());
        aoe.tiles[start..=end].fill(true);
        aoe.into()
    } else {
        Rectangle::tile(col as i32, row as i32, TILE_SIZEI).into()
    }
}

/// Merge another layer to this one
///
/// The other layer's opacity and blending mode are used.
///
/// The returned area of effect contains all the visible tiles of the source layer
pub fn merge_bitmap(target_layer: &mut BitmapLayer, source_layer: &BitmapLayer) -> AoE {
    assert_eq!(target_layer.size(), source_layer.size());

    let target_tiles = target_layer.tilevec_mut();
    let source_tiles = source_layer.tilevec();

    // TODO this is parallelizable
    target_tiles
        .iter_mut()
        .zip(source_tiles.iter())
        .for_each(|(d, s)| {
            d.merge(
                s,
                source_layer.metadata.opacity,
                source_layer.metadata.blendmode,
            )
        });

    if source_layer.metadata.is_visible() {
        source_layer.nonblank_tilemap().into()
    } else {
        AoE::Nothing
    }
}

/// Merge a group layer to a bitmap layer
///
/// This action flattens the group, then merges the flattened layer using
/// the group's blending mode.
/// If the group is non-isolated, each sublayer will be merged individually.
///
/// The returned area of effect contains all the visible tiles of the source group
pub fn merge_group(target_layer: &mut BitmapLayer, source_group: &GroupLayer) -> AoE {
    let mut aoe = AoE::Nothing;

    if source_group.metadata().isolated {
        let mut tmp = BitmapLayer::new(0, source_group.width(), source_group.height(), Tile::Blank);
        tmp.metadata_mut().opacity = source_group.metadata().opacity;
        tmp.metadata_mut().blendmode = source_group.metadata().blendmode;

        for layer in source_group.iter_layers().rev() {
            match layer {
                Layer::Group(g) => {
                    aoe = aoe.merge(merge_group(&mut tmp, g));
                }
                Layer::Bitmap(b) => {
                    aoe = aoe.merge(merge_bitmap(&mut tmp, b));
                }
            }
        }

        aoe = aoe.merge(merge_bitmap(target_layer, &tmp));
    } else {
        for layer in source_group.iter_layers().rev() {
            match layer {
                Layer::Group(g) => {
                    aoe = aoe.merge(merge_group(target_layer, g));
                }
                Layer::Bitmap(b) => {
                    aoe = aoe.merge(merge_bitmap(target_layer, b));
                }
            }
        }
    }

    aoe
}

/// Merge a sublayer
pub fn merge_sublayer(layer: &mut BitmapLayer, sublayer_id: LayerID) -> AoE {
    if let Some(sublayer) = layer.take_sublayer(sublayer_id) {
        merge_bitmap(layer, &sublayer)
    } else {
        AoE::Nothing
    }
}

/// Remove a sublayer without merging it
pub fn remove_sublayer(layer: &mut BitmapLayer, sublayer_id: LayerID) -> AoE {
    if let Some(sublayer) = layer.take_sublayer(sublayer_id) {
        sublayer.nonblank_tilemap().into()
    } else {
        AoE::Nothing
    }
}

pub fn merge_all_sublayers(layer: &mut BitmapLayer) -> AoE {
    let mut aoe = AoE::Nothing;
    let sublayers = layer.sublayer_ids();
    for id in sublayers {
        aoe = aoe.merge(merge_sublayer(layer, id));
    }

    aoe
}

pub fn change_attributes(
    layer: &mut Layer,
    sublayer: LayerID,
    opacity: f32,
    blend: Blendmode,
    censored: bool,
    isolated: bool,
) -> AoE {
    if sublayer != 0 {
        if let Layer::Bitmap(bl) = layer {
            let sl = bl.get_or_create_sublayer(sublayer);
            sl.metadata.blendmode = blend;
            sl.metadata.opacity = opacity;

            sl.nonblank_tilemap().into()
        } else {
            AoE::Nothing
        }
    } else {
        let md = layer.metadata_mut();
        md.blendmode = blend;
        md.opacity = opacity;
        md.censored = censored;
        md.isolated = isolated;

        layer.nonblank_tilemap().into()
    }
}

/// Move pixels from the source rectangle to the target position
///
/// Source rectangle must be within layer bounds.
/// Target rectangle may be (partially) out of bounds and
/// may overlap with the source
/// If a mask is given, it's length must be source width*height.
pub fn move_rect(
    layer: &mut BitmapLayer,
    user: UserID,
    source_rect: Rectangle,
    dest_x: i32,
    dest_y: i32,
    mask: Option<&[Pixel]>,
) -> AoE {
    // Copy the existing pixels.
    let mut src_image = match layer.to_image(source_rect) {
        Ok(i) => i,
        Err(_) => {
            return AoE::Nothing;
        }
    };

    // Clear out the existing pixels
    let aoe = if let Some(m) = mask {
        src_image
            .pixels
            .iter_mut()
            .zip(m.iter())
            .for_each(|(s, m)| {
                if m[ALPHA_CHANNEL] == 0 {
                    *s = ZERO_PIXEL
                }
            });

        draw_image(layer, user, m, &source_rect, 1.0, Blendmode::Erase)
    } else {
        fill_rect(
            layer,
            user,
            &Color::TRANSPARENT,
            Blendmode::Replace,
            &source_rect,
        )
    };

    // Draw the image onto the destination
    aoe.merge(draw_image(
        layer,
        user,
        &src_image.pixels,
        &Rectangle::new(dest_x, dest_y, source_rect.w, source_rect.h),
        1.0,
        Blendmode::Normal,
    ))
}

#[cfg(test)]
mod tests {
    use super::super::color::{WHITE_PIXEL, ZERO_PIXEL};
    use super::super::LayerMetadata;
    use super::*;

    use std::sync::Arc;

    #[test]
    fn test_fill_rect() {
        let mut layer = BitmapLayer::new(0, 200, 200, Tile::Blank);

        fill_rect(
            &mut layer,
            0,
            &Color::from_pixel(WHITE_PIXEL),
            Blendmode::Normal,
            &Rectangle::new(1, 1, 198, 198),
        );

        assert_eq!(layer.pixel_at(100, 0), ZERO_PIXEL);

        assert_eq!(layer.pixel_at(0, 1), ZERO_PIXEL);
        assert_eq!(layer.pixel_at(1, 1), WHITE_PIXEL);
        assert_eq!(layer.pixel_at(198, 1), WHITE_PIXEL);
        assert_eq!(layer.pixel_at(199, 1), ZERO_PIXEL);

        assert_eq!(layer.pixel_at(0, 198), ZERO_PIXEL);
        assert_eq!(layer.pixel_at(1, 198), WHITE_PIXEL);
        assert_eq!(layer.pixel_at(198, 198), WHITE_PIXEL);
        assert_eq!(layer.pixel_at(199, 198), ZERO_PIXEL);
        assert_eq!(layer.pixel_at(1, 199), ZERO_PIXEL);
    }

    #[test]
    fn test_draw_brush_dab() {
        let mut layer = BitmapLayer::new(0, 128, 128, Tile::Blank);
        let brush = BrushMask::new_round_pixel(4, 1.0);
        // Shape should look like this:
        // 0110
        // 1111
        // 1111
        // 0110

        // Dab right at the intersection of four tiles
        draw_brush_dab(
            &mut layer,
            0,
            62,
            62,
            &brush,
            &Color::rgb8(255, 255, 255),
            Blendmode::Normal,
        );

        assert_eq!(layer.pixel_at(62, 62), ZERO_PIXEL);
        assert_eq!(layer.pixel_at(63, 62), WHITE_PIXEL);
        assert_eq!(layer.pixel_at(64, 62), WHITE_PIXEL);
        assert_eq!(layer.pixel_at(65, 62), ZERO_PIXEL);

        assert_eq!(layer.pixel_at(62, 63), WHITE_PIXEL);
        assert_eq!(layer.pixel_at(63, 63), WHITE_PIXEL);
        assert_eq!(layer.pixel_at(64, 63), WHITE_PIXEL);
        assert_eq!(layer.pixel_at(65, 63), WHITE_PIXEL);

        assert_eq!(layer.pixel_at(62, 64), WHITE_PIXEL);
        assert_eq!(layer.pixel_at(63, 64), WHITE_PIXEL);
        assert_eq!(layer.pixel_at(64, 64), WHITE_PIXEL);
        assert_eq!(layer.pixel_at(65, 64), WHITE_PIXEL);

        assert_eq!(layer.pixel_at(62, 65), ZERO_PIXEL);
        assert_eq!(layer.pixel_at(63, 65), WHITE_PIXEL);
        assert_eq!(layer.pixel_at(64, 65), WHITE_PIXEL);
        assert_eq!(layer.pixel_at(65, 65), ZERO_PIXEL);
    }

    #[test]
    fn test_layer_merge() {
        let mut btm = BitmapLayer::new(0, 128, 128, Tile::new(&Color::rgb8(0, 0, 0), 0));
        let mut top = BitmapLayer::new(0, 128, 128, Tile::new(&Color::rgb8(255, 0, 0), 0));
        top.metadata_mut().opacity = 0.5;

        merge_bitmap(&mut btm, &top);

        assert_eq!(btm.pixel_at(0, 0), Color::rgb8(127, 0, 0).as_pixel());
    }

    #[test]
    fn test_group_merge() {
        let mut btm = BitmapLayer::new(0, 128, 128, Tile::new(&Color::rgb8(0, 0, 0), 0));
        let top = Arc::new(Layer::Bitmap(BitmapLayer::new(
            0,
            128,
            128,
            Tile::new(&Color::rgb8(255, 0, 0), 0),
        )));
        let grp = GroupLayer::from_parts(
            LayerMetadata {
                id: 0,
                title: String::new(),
                opacity: 0.5,
                hidden: false,
                censored: false,
                blendmode: Blendmode::Normal,
                isolated: true,
            },
            128,
            128,
            vec![top],
        );

        merge_group(&mut btm, &grp);

        assert_eq!(btm.pixel_at(0, 0), Color::rgb8(127, 0, 0).as_pixel());
    }
}
