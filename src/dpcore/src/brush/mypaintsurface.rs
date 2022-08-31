// This file is part of Drawpile.
// Copyright (C) 2022 askmeaboutloom
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

use crate::paint::{BitmapLayer, Color};
use dpmypaint as c;
use std::option::Option;

pub trait MyPaintTarget {
    fn add_dab(
        &mut self,
        x: f32,
        y: f32,
        radius: f32,
        hardness: f32,
        opacity: f32,
        r: f32,
        g: f32,
        b: f32,
        a: f32,
        aspect_ratio: f32,
        angle: f32,
        lock_alpha: f32,
        colorize: f32,
        posterize: f32,
        posterize_num: f32,
    );
}

#[repr(C)]
pub struct MyPaintSurface<'a> {
    parent: c::MyPaintSurface2,
    source: Option<&'a BitmapLayer>,
    target: &'a mut dyn MyPaintTarget,
}

unsafe extern "C" fn draw_dab_pigment(
    parent: *mut c::MyPaintSurface2,
    x: f32,
    y: f32,
    radius: f32,
    color_r: f32,
    color_g: f32,
    color_b: f32,
    opaque: f32,
    hardness: f32,
    alpha_eraser: f32,
    aspect_ratio: f32,
    angle: f32,
    lock_alpha: f32,
    colorize: f32,
    posterize: f32,
    posterize_num: f32,
    // Spectral painting just eats a lot of performance for a negligible
    // difference in the resulting visuals, so we don't support that.
    _paint: f32,
) -> ::std::os::raw::c_int {
    let surface = &mut *(parent as *mut MyPaintSurface);
    if surface.draw_dab(
        x,
        y,
        radius,
        color_r,
        color_g,
        color_b,
        opaque,
        hardness,
        alpha_eraser,
        aspect_ratio,
        angle,
        lock_alpha,
        colorize,
        posterize,
        posterize_num,
    ) {
        1
    } else {
        0
    }
}

unsafe extern "C" fn draw_dab(
    parent: *mut c::MyPaintSurface,
    x: f32,
    y: f32,
    radius: f32,
    color_r: f32,
    color_g: f32,
    color_b: f32,
    opaque: f32,
    hardness: f32,
    alpha_eraser: f32,
    aspect_ratio: f32,
    angle: f32,
    lock_alpha: f32,
    colorize: f32,
) -> ::std::os::raw::c_int {
    draw_dab_pigment(
        parent as *mut c::MyPaintSurface2,
        x,
        y,
        radius,
        color_r,
        color_g,
        color_b,
        opaque,
        hardness,
        alpha_eraser,
        aspect_ratio,
        angle,
        lock_alpha,
        colorize,
        0.0,
        0.0,
        0.0,
    )
}

unsafe extern "C" fn get_color_pigment(
    parent: *mut c::MyPaintSurface2,
    x: f32,
    y: f32,
    radius: f32,
    color_r: *mut f32,
    color_g: *mut f32,
    color_b: *mut f32,
    color_a: *mut f32,
    // No spectral painting support for the sake of performance.
    _pigment: f32,
) {
    let surface = &mut *(parent as *mut MyPaintSurface);
    let color = surface.get_color(x, y, radius);
    *color_r = color.r;
    *color_g = color.g;
    *color_b = color.b;
    *color_a = color.a;
}

unsafe extern "C" fn get_color(
    parent: *mut c::MyPaintSurface,
    x: f32,
    y: f32,
    radius: f32,
    color_r: *mut f32,
    color_g: *mut f32,
    color_b: *mut f32,
    color_a: *mut f32,
) {
    get_color_pigment(
        parent as *mut c::MyPaintSurface2,
        x,
        y,
        radius,
        color_r,
        color_g,
        color_b,
        color_a,
        0.0,
    )
}

unsafe extern "C" fn begin_atomic(_parent: *mut c::MyPaintSurface) {
    // Nothing to do
}

unsafe extern "C" fn end_atomic(_parent: *mut c::MyPaintSurface, _roi: *mut c::MyPaintRectangle) {
    // Nothing to do
}

unsafe extern "C" fn end_atomic_multi(
    _parent: *mut c::MyPaintSurface2,
    _roi: *mut c::MyPaintRectangles,
) {
    // Nothing to do
}

impl<'a> MyPaintSurface<'a> {
    pub fn new(source: Option<&'a BitmapLayer>, target: &'a mut dyn MyPaintTarget) -> Self {
        // The compiler gets confused between fn pointers and fn items here, so
        // all the functions have to be cast to their own type to make it work.
        let mut surface = MyPaintSurface {
            parent: c::MyPaintSurface2 {
                parent: c::MyPaintSurface {
                    draw_dab: Option::from(
                        draw_dab
                            as unsafe extern "C" fn(
                                *mut c::MyPaintSurface,
                                f32,
                                f32,
                                f32,
                                f32,
                                f32,
                                f32,
                                f32,
                                f32,
                                f32,
                                f32,
                                f32,
                                f32,
                                f32,
                            )
                                -> ::std::os::raw::c_int,
                    ),
                    get_color: Option::from(
                        get_color
                            as unsafe extern "C" fn(
                                *mut c::MyPaintSurface,
                                f32,
                                f32,
                                f32,
                                *mut f32,
                                *mut f32,
                                *mut f32,
                                *mut f32,
                            ),
                    ),
                    begin_atomic: Option::from(
                        begin_atomic as unsafe extern "C" fn(*mut c::MyPaintSurface),
                    ),
                    end_atomic: Option::from(
                        end_atomic
                            as unsafe extern "C" fn(
                                *mut c::MyPaintSurface,
                                roi: *mut c::MyPaintRectangle,
                            ),
                    ),
                    destroy: Option::None,
                    save_png: Option::None,
                    refcount: 1,
                },
                draw_dab_pigment: Option::from(
                    draw_dab_pigment
                        as unsafe extern "C" fn(
                            *mut c::MyPaintSurface2,
                            f32,
                            f32,
                            f32,
                            f32,
                            f32,
                            f32,
                            f32,
                            f32,
                            f32,
                            f32,
                            f32,
                            f32,
                            f32,
                            f32,
                            f32,
                            f32,
                        ) -> ::std::os::raw::c_int,
                ),
                get_color_pigment: Option::from(
                    get_color_pigment
                        as unsafe extern "C" fn(
                            *mut c::MyPaintSurface2,
                            f32,
                            f32,
                            f32,
                            *mut f32,
                            *mut f32,
                            *mut f32,
                            *mut f32,
                            f32,
                        ),
                ),
                end_atomic_multi: Option::from(
                    end_atomic_multi
                        as unsafe extern "C" fn(
                            *mut c::MyPaintSurface2,
                            roi: *mut c::MyPaintRectangles,
                        ),
                ),
            },
            source: source,
            target: target,
        };
        unsafe { c::mypaint_surface_init(&mut surface.parent.parent) }
        surface
    }

    pub unsafe fn get_raw(&mut self) -> *mut c::MyPaintSurface2 {
        &mut self.parent
    }

    fn draw_dab(
        &mut self,
        x: f32,
        y: f32,
        radius: f32,
        color_r: f32,
        color_g: f32,
        color_b: f32,
        opaque: f32,
        hardness: f32,
        alpha_eraser: f32,
        aspect_ratio: f32,
        angle: f32,
        lock_alpha: f32,
        colorize: f32,
        posterize: f32,
        posterize_num: f32,
    ) -> bool {
        // A radius less than 0.1 pixels is infinitesimally small. A hardness or
        // opacity of zero means the mask is completely blank. Disregard the dab
        // in those cases. MyPaint uses the same logic for this.
        if radius >= 0.1 && hardness > 0.0 && opaque > 0.0 {
            self.target.add_dab(
                x,
                y,
                radius,
                hardness,
                opaque,
                color_r,
                color_g,
                color_b,
                alpha_eraser,
                aspect_ratio,
                angle,
                lock_alpha,
                colorize,
                posterize,
                posterize_num,
            );
            true
        } else {
            false
        }
    }

    fn get_color(&mut self, x: f32, y: f32, radius: f32) -> Color {
        match self.source {
            Some(bitmap_layer) => {
                bitmap_layer.sample_color(x as i32, y as i32, (radius * 2.0) as i32)
            }
            None => Color::TRANSPARENT,
        }
    }
}
