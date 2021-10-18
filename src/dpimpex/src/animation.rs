use std::fs::File;
use std::path::{Path, PathBuf};
use std::sync::Arc;
use std::time::Duration;

use super::conv::from_dpimage;
use crate::{ImageExportResult, ImpexError};
use dpcore::paint::{Image, LayerStack, LayerViewOptions};

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
    pos: isize,
    default_frame_duration: Duration,
}

impl<W: FrameWriter> AnimationSaver<W> {
    pub fn new(layerstack: Arc<LayerStack>, writer: W) -> Self {
        let default_frame_duration =
            Duration::from_millis(1000 / layerstack.metadata().framerate.max(1) as u64);
        Self {
            layerstack,
            writer,
            pos: 0,
            default_frame_duration,
        }
    }
}

impl<W: FrameWriter> AnimationWriter for AnimationSaver<W> {
    /// Write the next frame
    ///
    /// Note: this might not actually write anything if the next frame to be written is
    /// hidden or a fixed layer.
    ///
    /// Returns false when there are no more frames left to write
    fn save_next_frame(&mut self) -> Result<bool, ImpexError> {
        let image = self
            .layerstack
            .to_image(&LayerViewOptions::frame(self.layerstack.frame_at(self.pos)));
        self.writer
            .write_frame(&image, self.default_frame_duration)?;

        if self.pos >= self.layerstack.frame_count() as isize - 1 {
            Ok(false)
        } else {
            self.pos += 1;
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
            next_number: 1,
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
