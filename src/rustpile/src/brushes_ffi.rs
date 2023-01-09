use super::paintengine_ffi::PaintEngine;
use dpcore::brush::{
    BrushEngine, BrushState, ClassicBrush, ClassicBrushShape, MyPaintBrush, MyPaintSettings,
};
use dpcore::paint::rectiter::{MutableRectIterator, RectIterator};
use dpcore::paint::{BrushMask, ClassicBrushCache, Color, LayerID, Pixel8, Rectangle, Size};
use dpcore::protocol::MessageWriter;

use std::slice;

#[no_mangle]
pub extern "C" fn brushengine_new() -> *mut BrushEngine {
    Box::into_raw(Box::new(BrushEngine::new()))
}

#[no_mangle]
pub extern "C" fn brushengine_free(be: *mut BrushEngine) {
    if !be.is_null() {
        unsafe { Box::from_raw(be) };
    }
}

#[no_mangle]
pub extern "C" fn brushengine_set_classicbrush(
    be: &mut BrushEngine,
    brush: &ClassicBrush,
    layer: u16,
) {
    be.set_classicbrush(*brush);
    be.set_layer(layer);
}

#[no_mangle]
pub extern "C" fn brushengine_set_mypaintbrush(
    be: &mut BrushEngine,
    brush: &MyPaintBrush,
    settings: &MyPaintSettings,
    layer: u16,
    freehand: bool,
) {
    be.set_mypaintbrush(brush, settings, freehand);
    be.set_layer(layer);
}

#[no_mangle]
pub extern "C" fn brushengine_stroke_to(
    be: &mut BrushEngine,
    x: f32,
    y: f32,
    p: f32,
    delta_msec: i64,
    pe: Option<&PaintEngine>,
    layer_id: LayerID,
) {
    if let Some(paintengine) = pe {
        let vc = paintengine.viewcache.lock().unwrap();
        be.stroke_to(
            x,
            y,
            p,
            delta_msec,
            vc.layerstack.root().get_bitmaplayer(layer_id),
        );
    } else {
        be.stroke_to(x, y, p, delta_msec, None);
    }
}

#[no_mangle]
pub extern "C" fn brushengine_end_stroke(be: &mut BrushEngine) {
    be.end_stroke();
}

#[no_mangle]
pub extern "C" fn brushengine_add_offset(be: &mut BrushEngine, x: f32, y: f32) {
    be.add_offset(x, y);
}

#[no_mangle]
pub extern "C" fn brushengine_write_dabs(
    be: &mut BrushEngine,
    user_id: u8,
    writer: &mut MessageWriter,
) {
    be.write_dabs(user_id, writer)
}

#[no_mangle]
pub extern "C" fn brush_preview_dab(
    brush: &ClassicBrush,
    image: *mut u8,
    width: i32,
    height: i32,
    color: &Color,
) {
    let pixel_slice =
        unsafe { slice::from_raw_parts_mut(image as *mut Pixel8, (width * height) as usize) };

    let mask = match brush.shape {
        ClassicBrushShape::RoundPixel => BrushMask::new_round_pixel(brush.size.1 as u32),
        ClassicBrushShape::SquarePixel => BrushMask::new_square_pixel(brush.size.1 as u32),
        ClassicBrushShape::RoundSoft => {
            let mut cache = ClassicBrushCache::new();
            BrushMask::new_gimp_style_v2(0.0, 0.0, brush.size.1, brush.hardness.1, &mut cache).2
        }
    };

    let d = mask.diameter as i32;

    let dest_rect = Rectangle::new(width / 2 - d / 2, height / 2 - d / 2, d, d)
        .cropped(Size { width, height })
        .unwrap();

    let source_rect = Rectangle::new(
        d / 2 - dest_rect.w / 2,
        d / 2 - dest_rect.h / 2,
        dest_rect.w,
        dest_rect.h,
    );

    MutableRectIterator::from_rectangle(pixel_slice, width as usize, &dest_rect)
        .zip(RectIterator::from_rectangle(
            &mask.mask,
            mask.diameter as usize,
            &source_rect,
        ))
        .for_each(|(d, s)| {
            d.iter_mut().zip(s).for_each(|(pix, &val)| {
                *pix = Color {
                    r: color.r,
                    g: color.g,
                    b: color.b,
                    a: val as f32 / 255.0 * brush.opacity.1,
                }
                .as_pixel8()
            });
        });
}
