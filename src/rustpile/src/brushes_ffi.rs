use super::paintengine_ffi::PaintEngine;
use dpcore::brush::{BrushEngine, BrushState, ClassicBrush};
use dpcore::paint::LayerID;
use dpcore::protocol::MessageWriter;

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
    be.set_classicbrush(brush.clone());
    be.set_layer(layer);
}

#[no_mangle]
pub extern "C" fn brushengine_stroke_to(
    be: &mut BrushEngine,
    x: f32,
    y: f32,
    p: f32,
    pe: Option<&PaintEngine>,
    layer_id: LayerID,
) {
    if let Some(paintengine) = pe {
        let vc = paintengine.viewcache.lock().unwrap();
        be.stroke_to(x, y, p, vc.layerstack.get_layer(layer_id));
    } else {
        be.stroke_to(x, y, p, None);
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
