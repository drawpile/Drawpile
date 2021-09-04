use std::path::{Path, PathBuf};
use std::fs::File;

use super::conv::from_dpimage;
use crate::ImageExportResult;
use dpcore::paint::{LayerStack, LayerViewOptions};

use image::codecs::gif::{GifEncoder, Repeat};
use image::{Frame, Delay};

pub fn save_gif_animation(path: &Path, layerstack: &LayerStack) -> ImageExportResult {
    let mut encoder = GifEncoder::new(File::create(path)?);

    encoder.set_repeat(Repeat::Infinite)?;

    for (idx, layer) in layerstack.iter_layers().enumerate() {
        if !layer.fixed && layer.is_visible() {
            let image = layerstack.to_image(&LayerViewOptions::solo(idx));
            // TODO subframe delta
            encoder.encode_frame(
                Frame::from_parts(
                    from_dpimage(&image),
                    0,
                    0,
                    Delay::from_numer_denom_ms(41, 1), // TODO adjustable FPS
                )
            )?;
        }
    }
    Ok(())
}

pub fn save_frames_animation(path: &Path, layerstack: &LayerStack) -> ImageExportResult {
    let mut framenum = 1;
    for (idx, layer) in layerstack.iter_layers().enumerate() {
        if !layer.fixed && layer.is_visible() {
            let mut pb = PathBuf::new();
            pb.push(path);
            pb.push(format!("frame-{:03}.png", framenum));

            let image = from_dpimage(&layerstack.to_image(&LayerViewOptions::solo(idx)));
            image.save(pb)?;
            framenum += 1;
        }
    }

    Ok(())
}