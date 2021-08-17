use core::ffi::c_void;
use dpcore::paint::annotation::{Annotation, AnnotationID, VAlign};
use dpcore::paint::{Blendmode, Color, Layer, Rectangle};
use std::os::raw::c_char;
use std::sync::Arc;

/// Layer's properties for updating a layer list in the GUI
#[repr(C)]
pub struct LayerInfo {
    pub title: *const u8,
    pub titlelen: i32,
    pub opacity: f32,
    pub id: i32,
    pub hidden: bool,
    pub censored: bool,
    pub fixed: bool,
    pub blendmode: Blendmode,
}

impl From<&Layer> for LayerInfo {
    fn from(item: &Layer) -> Self {
        Self {
            title: item.title.as_ptr(),
            titlelen: item.title.len() as i32,
            opacity: item.opacity,
            id: item.id,
            hidden: item.hidden,
            censored: item.censored,
            fixed: item.fixed,
            blendmode: item.blendmode,
        }
    }
}

/// A snapshot of annotations for updating their GUI representations
///
/// This struct is thread-safe and can be passed to the main thread
/// from the paint engine thread.
pub struct Annotations(pub Arc<Vec<Arc<Annotation>>>);

type UpdateAnnotationCallback = extern "C" fn(
    ctx: *mut c_void,
    id: AnnotationID,
    text: *const c_char,
    textlen: usize,
    rect: Rectangle,
    background: Color,
    protect: bool,
    valign: VAlign,
);

#[no_mangle]
pub extern "C" fn annotations_get_all(
    annotations: &Annotations,
    ctx: *mut c_void,
    callback: UpdateAnnotationCallback,
) {
    for a in annotations.0.iter() {
        callback(
            ctx,
            a.id,
            a.text.as_ptr() as *const c_char,
            a.text.len(),
            a.rect,
            a.background,
            a.protect,
            a.valign,
        );
    }
}

#[no_mangle]
pub extern "C" fn annotations_free(annotations: *mut Annotations) {
    if !annotations.is_null() {
        unsafe { Box::from_raw(annotations) };
    }
}

/// The result of an "annotation at point" query
#[repr(C)]
pub struct AnnotationAt {
    /// ID of the annotation at the queried point.
    /// Will be zero if none was found
    pub id: AnnotationID,

    /// The shape of the annotation
    pub rect: Rectangle,

    /// The annotation's protected bit
    pub protect: bool,
}
