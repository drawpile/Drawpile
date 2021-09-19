use dpcore::paint::{Size, Rectangle, Blendmode};

mod reader;
mod writer;

pub use reader::load_openraster_image;
pub use writer::save_openraster_image;

static MYPAINT_NAMESPACE: &str = "http://mypaint.org/ns/openraster";
static DP_NAMESPACE: &str = "http://drawpile.net/";

struct OraCanvas {
    size: Size,
    unsupported_features: bool,
    dpi: (i32, i32),
    root: OraStack,
    annotations: Vec<OraAnnotation>, // our extension
}

enum Isolation {
    Isolate,
    Auto, // Isolate if opacity < 1.0 or composite_op != src-over
}

struct OraCommon {
    name: String,
    offset: (i32, i32),
    opacity: f32,
    visibility: bool,
    locked: bool,
    censored: bool, // our extension
    fixed: bool,    // our extension
    composite_op: Blendmode,
    unsupported_features: bool, // this layer makes use of unsupported features
}

struct OraLayer {
    common: OraCommon,
    filename: String,
    bgtile: String, // MyPaint extension
}

struct OraStack {
    common: OraCommon,
    isolation: Isolation,

    layers: Vec<OraStackElement>,
    annotations: Vec<OraAnnotation>, // backward compatibility for ORA files written by Drawpile <2.2
}

enum OraStackElement {
    Stack(OraStack),
    Layer(OraLayer),
}

impl OraStackElement {
    fn element_name(&self) -> &'static str {
        match self {
            OraStackElement::Stack(_) => "stack",
            OraStackElement::Layer(_) => "layer",
        }
    }
}

struct OraAnnotation {
    rect: Rectangle,
    bg: String,
    valign: String,
    content: String,
}