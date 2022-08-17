extern crate cc;

fn main() {
    cc::Build::new()
        .include("bundled/libmypaint")
        .file("bundled/libmypaint/helpers.c")
        .file("bundled/libmypaint/mypaint-brush.c")
        .file("bundled/libmypaint/mypaint-brush-settings.c")
        .file("bundled/libmypaint/mypaint.c")
        .file("bundled/libmypaint/mypaint-mapping.c")
        .file("bundled/libmypaint/mypaint-rectangle.c")
        .file("bundled/libmypaint/mypaint-surface.c")
        .file("bundled/libmypaint/rng-double.c")
        .compile("libmypaint.a")
}
