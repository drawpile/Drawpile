use dpcore::paint::{Blendmode, Layer};

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
