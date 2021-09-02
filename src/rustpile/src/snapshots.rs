use dpcore::paint::{LayerStack, LayerViewOptions, Pixel, Rectangle, Size};

use std::collections::VecDeque;
use std::slice;
use std::sync::Arc;
use std::time::{Duration, Instant};

#[derive(Clone)]
pub struct Snapshot {
    pub layerstack: Arc<LayerStack>,
    timestamp: Instant,
}

#[derive(Clone)]
pub struct SnapshotQueue {
    snapshots: Arc<VecDeque<Snapshot>>,
    min_interval: Duration,
    max_snapshots: usize,
}

impl SnapshotQueue {
    pub fn new(min_interval: Duration, max_snapshots: usize) -> SnapshotQueue {
        Self {
            snapshots: Arc::new(VecDeque::with_capacity(max_snapshots)),
            min_interval,
            max_snapshots,
        }
    }

    /// Add a snapshot if sufficient time has elapsed since the last
    /// one was added.
    pub fn add(&mut self, layerstack: &Arc<LayerStack>) {
        if let Some(b) = self.snapshots.back() {
            if b.timestamp.elapsed() < self.min_interval {
                return;
            }
        }

        let snapshots = Arc::make_mut(&mut self.snapshots);

        if snapshots.len() >= self.max_snapshots {
            snapshots.pop_front();
        }

        snapshots.push_back(Snapshot {
            layerstack: layerstack.clone(),
            timestamp: Instant::now(),
        });
    }

    pub fn get(&self, index: usize) -> Option<&LayerStack> {
        self.snapshots.get(index).map(|s| s.layerstack.as_ref())
    }
}

#[no_mangle]
pub extern "C" fn snapshots_count(snapshots: &SnapshotQueue) -> u32 {
    snapshots.snapshots.len() as u32
}

#[no_mangle]
pub extern "C" fn snapshots_size(snapshots: &SnapshotQueue, index: usize) -> Size {
    snapshots.snapshots.get(index).map_or(
        Size {
            width: 0,
            height: 0,
        },
        |s| s.layerstack.size(),
    )
}

#[no_mangle]
pub extern "C" fn snapshots_get_content(
    snapshots: &SnapshotQueue,
    index: usize,
    pixels: *mut u8,
) -> bool {
    if let Some(s) = snapshots.snapshots.get(index) {
        let size = s.layerstack.size();
        let pixel_slice = unsafe {
            slice::from_raw_parts_mut(pixels as *mut Pixel, (size.width * size.height) as usize)
        };

        s.layerstack
            .to_pixels(
                Rectangle::new(0, 0, size.width, size.height),
                &LayerViewOptions::default(),
                pixel_slice,
            )
            .unwrap();

        true
    } else {
        false
    }
}
