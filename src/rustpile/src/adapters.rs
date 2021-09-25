use core::ffi::c_void;
use dpcore::paint::annotation::{Annotation, AnnotationID, VAlign};
use dpcore::paint::{Blendmode, Color, GroupLayer, Layer, LayerMetadata, Rectangle, RootGroup};

use std::os::raw::c_char;
use std::sync::Arc;

/// Layer's properties for updating a layer list in the GUI
#[repr(C)]
pub struct LayerInfo {
    pub title: *const u8,
    pub titlelen: i32,
    pub opacity: f32,
    pub id: u16,
    pub hidden: bool,
    pub censored: bool,
    pub fixed: bool,
    pub isolated: bool,
    pub group: bool,
    pub blendmode: Blendmode,

    // number of child items this layer has. Zero for all non-group layers.
    pub children: u16,

    // index in parent group
    pub rel_index: u16,

    // left and right indices (modified preorder tree traversal)
    pub left: i32,
    pub right: i32,
}

/// Return a flat list describing a depth-first traveresal of the layer tree
pub fn flatten_layerinfo(root: &RootGroup) -> Vec<LayerInfo> {
    let mut list: Vec<LayerInfo> = Vec::new();

    fn flatten(list: &mut Vec<LayerInfo>, index: &mut i32, group: &GroupLayer) {
        // Note: layers are listed top-to-bottom in the GUI
        for (i, l) in group.iter_layers().enumerate() {
            match l {
                Layer::Group(g) => {
                    let info = LayerInfo::new(
                        g.metadata(),
                        true,
                        g.layer_count() as u16,
                        i as u16,
                        *index,
                        -1,
                    );
                    *index += 1;
                    let pos = list.len();
                    list.push(info);
                    flatten(list, index, g);
                    list[pos].right = *index;
                    *index += 1;
                }
                Layer::Bitmap(l) => {
                    list.push(LayerInfo::new(
                        l.metadata(),
                        false,
                        0,
                        i as u16,
                        *index,
                        *index + 1,
                    ));
                    *index += 2;
                }
            }
        }
    }

    let mut index = 0;
    flatten(&mut list, &mut index, root.inner_ref());

    return list;
}

impl LayerInfo {
    fn new(
        md: &LayerMetadata,
        group: bool,
        children: u16,
        rel_index: u16,
        left: i32,
        right: i32,
    ) -> Self {
        Self {
            title: md.title.as_ptr(),
            titlelen: md.title.len() as i32,
            opacity: md.opacity,
            id: md.id,
            hidden: md.hidden,
            censored: md.censored,
            fixed: md.fixed,
            blendmode: md.blendmode,
            isolated: md.isolated,
            group,
            children,
            rel_index,
            left,
            right,
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
