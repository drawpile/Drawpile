use std::fs::File;
use std::path::{Path, PathBuf};
use std::sync::Arc;
use std::time::Duration;

use super::conv::from_dpimage;
use crate::{ImageExportResult, ImpexError};
use dpcore::paint::{LayerStack, LayerViewOptions, Image};

use image::codecs::gif::{GifEncoder, Repeat};
use image::{Delay, Frame};

pub trait AnimationWriter {
    fn save_next_frame(&mut self) -> Result<bool, ImpexError>;
}

pub trait FrameWriter {
    fn write_frame(&mut self, frame: &Image, duration: Duration) -> ImageExportResult;
}

pub struct AnimationSaver<W: FrameWriter> {
    layerstack: Arc<LayerStack>,
    writer: W,
    pos: usize,
    default_frame_duration: Duration,
}

impl <W: FrameWriter> AnimationSaver<W> {
    pub fn new(layerstack: Arc<LayerStack>, writer: W) -> Self {
        // The bottom-most layer is the first frame, which at the end of the list
        assert!(layerstack.root().layer_count() > 0 );
        let pos = layerstack.root().layer_count() - 1;
        let default_frame_duration = Duration::from_millis(1000 / layerstack.metadata().framerate.max(1) as u64);
        Self { layerstack, writer, pos, default_frame_duration }
    }

}

impl <W: FrameWriter> AnimationWriter for AnimationSaver<W> {
    /// Write the next frame
    ///
    /// Note: this might not actually write anything if the next frame to be written is
    /// hidden or a fixed layer.
    ///
    /// Returns false when there are no more frames left to write
    fn save_next_frame(&mut self) -> Result<bool, ImpexError> {
        let layer = self.layerstack.root().inner_ref().layer_at(self.pos);
        // Fixed layers' content is drawn on all frames
        if !layer.metadata().fixed && layer.is_visible() {
            let image = self.layerstack.to_image(&LayerViewOptions::frame(self.pos));
            self.writer.write_frame(&image, self.default_frame_duration)?;
        }

        if self.pos == 0 {
            Ok(false)
        } else {
            self.pos -= 1;
            Ok(true)
        }
    }
}

pub struct GifWriter {
    encoder: GifEncoder<File>,
}

impl GifWriter {
    pub fn new(path: &Path) -> Result<Self, ImpexError> {
        let mut encoder = GifEncoder::new(File::create(path)?);
        encoder.set_repeat(Repeat::Infinite)?;

        Ok(Self { encoder })
    }
}

impl FrameWriter for GifWriter {
    fn write_frame(&mut self, frame: &Image, duration: Duration) -> ImageExportResult {
        // TODO subframe delta
        self.encoder.encode_frame(Frame::from_parts(
            from_dpimage(frame),
            0,
            0,
            Delay::from_saturating_duration(duration),
        ))?;
        Ok(())
    }
}

pub struct FrameImagesWriter {
    dir: PathBuf,
    next_number: i32,
}

impl FrameImagesWriter {
    pub fn new(path: &Path) -> Self {
        let mut dir = PathBuf::new();
        dir.push(path);
        Self {
            dir,
            next_number: 1
        }
    }
}

impl FrameWriter for FrameImagesWriter {
    fn write_frame(&mut self, frame: &Image, _duration: Duration) -> ImageExportResult {
        let mut pb = self.dir.clone();
        pb.push(format!("frame-{:03}.png", self.next_number));

        from_dpimage(frame).save(pb)?;
        self.next_number += 1;

        Ok(())
    }
}
