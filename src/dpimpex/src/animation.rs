use std::fs::File;
use std::path::{Path, PathBuf};

use super::conv::from_dpimage;
use crate::ImageExportResult;
use dpcore::paint::{LayerStack, LayerViewOptions};

use image::codecs::gif::{GifEncoder, Repeat};
use image::{Delay, Frame};

pub fn save_gif_animation(path: &Path, layerstack: &LayerStack) -> ImageExportResult {
    let mut encoder = GifEncoder::new(File::create(path)?);

    encoder.set_repeat(Repeat::Infinite)?;

    let last = layerstack.root().layer_count() - 1;

    let delay = Delay::from_numer_denom_ms(1000, layerstack.metadata().framerate.max(1) as u32);

    for (idx, layer) in layerstack.root().iter_layers().rev().enumerate() {
        if !layer.metadata().fixed && layer.is_visible() {
            let image = layerstack.to_image(&LayerViewOptions::frame(last - idx));
            // TODO subframe delta
            encoder.encode_frame(Frame::from_parts(from_dpimage(&image), 0, 0, delay))?;
        }
    }
    Ok(())
}

pub fn save_frames_animation(path: &Path, layerstack: &LayerStack) -> ImageExportResult {
    let last = layerstack.root().layer_count() - 1;
    let mut framenum = 1;
    for (idx, layer) in layerstack.root().iter_layers().rev().enumerate() {
        if !layer.metadata().fixed && layer.is_visible() {
            let mut pb = PathBuf::new();
            pb.push(path);
            pb.push(format!("frame-{:03}.png", framenum));

            let image = from_dpimage(&layerstack.to_image(&LayerViewOptions::frame(last - idx)));
            image.save(pb)?;
            framenum += 1;
        }
    }

    Ok(())
}
