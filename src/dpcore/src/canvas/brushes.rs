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

use crate::paint::{
    editlayer, AoE, BitmapLayer, Blendmode, BrushMask, ClassicBrushCache, Color, UserID,
};
use crate::protocol::message::{
    DrawDabsClassicMessage, DrawDabsMyPaintMessage, DrawDabsPixelMessage,
};

use std::convert::TryFrom;

/// Draw dabs using a Classic brush
///
/// If the color has a non-zero alpha component, the dabs will be drawn
/// to a sublayer
///
/// If the user ID is zero, the dabs will always be drawn directly
/// onto the layer with Normal blend mode
pub fn drawdabs_classic(
    layer: &mut BitmapLayer,
    user: UserID,
    dabs: &DrawDabsClassicMessage,
    cache: &mut ClassicBrushCache,
) -> (AoE, (i32, i32)) {
    let mode = if user != 0 {
        Blendmode::try_from(dabs.mode).unwrap_or_default()
    } else {
        Blendmode::Normal
    };

    let mut color = Color::from_argb32(dabs.color);

    let result = if color.a > 0.0 && user != 0 {
        // If alpha is given, these dabs will be drawn in indirect mode
        let sublayer = layer.get_or_create_sublayer(user.into());
        sublayer.metadata.opacity = color.a;
        sublayer.metadata.blendmode = mode;
        color.a = 1.0;
        drawdabs_classic_draw(sublayer, user, color, Blendmode::Normal, dabs, cache)
    } else {
        color.a = 1.0; // needed because as_pixel returns premultiplied pixel values
        drawdabs_classic_draw(layer, user, color, mode, dabs, cache)
    };

    if mode.can_decrease_opacity() {
        layer.optimize(&result.0);
    }

    result
}

fn drawdabs_classic_draw(
    layer: &mut BitmapLayer,
    user: UserID,
    color: Color,
    mode: Blendmode,
    dabs: &DrawDabsClassicMessage,
    cache: &mut ClassicBrushCache,
) -> (AoE, (i32, i32)) {
    let mut last_x = dabs.x;
    let mut last_y = dabs.y;
    let mut aoe = AoE::Nothing;
    for dab in dabs.dabs.iter() {
        let x = last_x + dab.x as i32;
        let y = last_y + dab.y as i32;

        let (mx, my, mask) = BrushMask::new_gimp_style_v2(
            x as f32 / 4.0,
            y as f32 / 4.0,
            dab.size as f32 / 256.0,
            dab.hardness as f32 / 255.0,
            cache,
        );
        aoe = aoe.merge(editlayer::draw_brush_dab(
            layer, user, mx, my, &mask, &color, mode, dab.opacity,
        ));

        last_x = x;
        last_y = y;
    }

    (aoe, (last_x / 4, last_y / 4))
}

/// Draw dabs using a Pixel brush
///
/// If the color has a non-zero alpha component, the dabs will be drawn
/// to a sublayer
///
/// If the user ID is zero, the dabs will always be drawn directly
/// onto the layer with Normal blend mode
pub fn drawdabs_pixel(
    layer: &mut BitmapLayer,
    user: UserID,
    dabs: &DrawDabsPixelMessage,
    square: bool,
) -> (AoE, (i32, i32)) {
    let mode = if user != 0 {
        Blendmode::try_from(dabs.mode).unwrap_or_default()
    } else {
        Blendmode::Normal
    };

    let mut color = Color::from_argb32(dabs.color);

    let result = if color.a > 0.0 && user != 0 {
        // If alpha is given, these dabs will be drawn in indirect mode
        let sublayer = layer.get_or_create_sublayer(user.into());
        sublayer.metadata.opacity = color.a;
        sublayer.metadata.blendmode = mode;
        color.a = 1.0;
        drawdabs_pixel_draw(sublayer, user, color, Blendmode::Normal, dabs, square)
    } else {
        color.a = 1.0; // needed because as_pixel returns premultiplied pixel values
        drawdabs_pixel_draw(layer, user, color, mode, dabs, square)
    };

    if mode.can_decrease_opacity() {
        layer.optimize(&result.0);
    }

    result
}

fn drawdabs_pixel_draw(
    layer: &mut BitmapLayer,
    user: UserID,
    color: Color,
    mode: Blendmode,
    dabs: &DrawDabsPixelMessage,
    square: bool,
) -> (AoE, (i32, i32)) {
    let mut mask = BrushMask {
        diameter: 0,
        mask: Vec::new(),
    };

    let mut last_x = dabs.x;
    let mut last_y = dabs.y;
    let mut last_size = 0;
    let mut aoe = AoE::Nothing;

    for dab in dabs.dabs.iter() {
        let x = last_x + dab.x as i32;
        let y = last_y + dab.y as i32;

        if dab.size != last_size {
            last_size = dab.size;
            mask = if square {
                BrushMask::new_square_pixel(dab.size as u32)
            } else {
                BrushMask::new_round_pixel(dab.size as u32)
            };
        }

        let offset = dab.size as i32 / 2;
        aoe = aoe.merge(editlayer::draw_brush_dab(
            layer,
            user,
            x - offset,
            y - offset,
            &mask,
            &color,
            mode,
            dab.opacity,
        ));

        last_x = x;
        last_y = y;
    }

    (aoe, (last_x, last_y))
}

pub fn drawdabs_mypaint(
    layer: &mut BitmapLayer,
    user: UserID,
    dabs: &DrawDabsMyPaintMessage,
) -> (AoE, (i32, i32)) {
    let (result, may_have_decreased_opacity) = drawdabs_mypaint_draw(layer, user, dabs);

    if may_have_decreased_opacity {
        layer.optimize(&result.0);
    }

    result
}

fn drawdabs_mypaint_draw(
    layer: &mut BitmapLayer,
    user: UserID,
    dabs: &DrawDabsMyPaintMessage,
) -> ((AoE, (i32, i32)), bool) {
    let mut aoe = AoE::Nothing;
    let color = Color::from_argb32(dabs.color);

    let lock_alpha = dabs.lock_alpha as f32 / 255.0f32;
    let normal = 1.0f32 * (1.0f32 - lock_alpha);

    let mut last_x = dabs.x;
    let mut last_y = dabs.y;
    let mut may_have_decreased_opacity = false;

    for dab in dabs.dabs.iter() {
        let x = last_x + dab.x as i32;
        let y = last_y + dab.y as i32;
        last_x = x;
        last_y = y;

        let (mx, my, mask) = BrushMask::new_mypaint_brush_mask(
            x as f32 / 4.0f32,
            y as f32 / 4.0f32,
            dab.size as f32 / 256.0f32,
            dab.hardness as f32 / 255.0f32,
            // See aspect_ratio_to_u8 in mypaintbrushstate.
            if dab.aspect_ratio == 23 {
                1.0f32 // Fudge this to be a perfectly round dab.
            } else {
                dab.aspect_ratio as f32 / 25.755f32 + 0.1f32
            },
            dab.angle as f32 / 255.0f32 * 360.0f32,
        );

        let opacity = dab.opacity as f32 / 255.0f32;

        if normal > 0.0f32 {
            let mode = if color.a == 1.0 {
                Blendmode::Normal
            } else {
                may_have_decreased_opacity = true;
                Blendmode::NormalAndEraser
            };
            aoe = aoe.merge(editlayer::draw_brush_dab(
                layer,
                user,
                mx,
                my,
                &mask,
                &color,
                mode,
                (normal * opacity * 255.0) as u8,
            ));
        }

        if lock_alpha > 0.0f32 && color.a != 0.0 {
            aoe = aoe.merge(editlayer::draw_brush_dab(
                layer,
                user,
                mx,
                my,
                &mask,
                &color,
                Blendmode::Recolor,
                (lock_alpha * opacity * 255.0) as u8,
            ));
        }

        // Colorize, posterize and spectral painting isn't supported while we
        // still only have 8 bits to work with. If we end up expanding that to
        // 15 bits like MyPaint does it, it'd be worth implementing.
    }

    ((aoe, (last_x / 4, last_y / 4)), may_have_decreased_opacity)
}
