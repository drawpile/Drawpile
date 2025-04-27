use super::{AclState, DrawContext, Image, Player};
use crate::{
    dp_error_anyhow, msg::Message, DP_AnnotationList, DP_CanvasState, DP_DocumentMetadata,
    DP_ImageScaleInterpolation, DP_LayerPropsList, DP_Message, DP_PaintEngine, DP_Pixel8,
    DP_PlayerResult, DP_Rect, DP_SaveImageType, DP_SelectionSet, DP_Timeline,
    DP_affected_area_in_bounds, DP_affected_area_make, DP_canvas_state_decref, DP_message_type,
    DP_paint_engine_free_join, DP_paint_engine_handle_inc, DP_paint_engine_new_inc,
    DP_paint_engine_playback_begin, DP_paint_engine_playback_play,
    DP_paint_engine_playback_skip_by, DP_paint_engine_playback_step,
    DP_paint_engine_render_everything, DP_paint_engine_reveal_censored_set, DP_paint_engine_tick,
    DP_paint_engine_view_canvas_state_inc, DP_save, DP_MSG_INTERVAL, DP_MSG_UNDO,
    DP_PAINT_ENGINE_FILTER_MESSAGE_FLAG_NO_TIME, DP_PLAYER_RECORDING_END, DP_PLAYER_SUCCESS,
    DP_SAVE_RESULT_SUCCESS, DP_TILE_SIZE,
};
use anyhow::Result;
use std::{
    ffi::{c_int, c_longlong, c_uint, c_void, CString},
    ptr::{self, null_mut},
    sync::{
        mpsc::{sync_channel, Receiver, SyncSender},
        Barrier,
    },
    time::{SystemTime, UNIX_EPOCH},
};

pub struct PaintEngine {
    paint_dc: DrawContext,
    main_dc: DrawContext,
    preview_dc: DrawContext,
    acls: AclState,
    paint_engine: *mut DP_PaintEngine,
    render_barrier: Barrier,
    render_width: usize,
    render_height: usize,
    render_image: Vec<u32>,
    playback_channel: (SyncSender<c_longlong>, Receiver<c_longlong>),
}

pub type PlaybackCrop = (usize, usize, usize, usize);

struct PlaybackParams {
    messages: Vec<Message>,
    crop: Option<PlaybackCrop>,
}

impl PaintEngine {
    const TILE_SIZE: usize = DP_TILE_SIZE as usize;

    pub fn new(player: Option<Player>) -> Box<Self> {
        let mut pe = Box::new(Self {
            paint_dc: DrawContext::default(),
            main_dc: DrawContext::default(),
            preview_dc: DrawContext::default(),
            acls: AclState::default(),
            paint_engine: ptr::null_mut(),
            render_barrier: Barrier::new(2),
            render_width: 0,
            render_height: 0,
            render_image: Vec::new(),
            playback_channel: sync_channel(1),
        });
        let user: *mut Self = &mut *pe;
        pe.paint_engine = unsafe {
            DP_paint_engine_new_inc(
                pe.paint_dc.as_ptr(),
                pe.main_dc.as_ptr(),
                pe.preview_dc.as_ptr(),
                pe.acls.as_ptr(),
                ptr::null_mut(),
                true,
                0xff646464u32,
                0xff878787u32,
                0x0u32,
                Some(Self::on_renderer_tile),
                Some(Self::on_renderer_unlock),
                Some(Self::on_renderer_resize),
                user.cast(),
                Some(Self::on_save_point),
                user.cast(),
                None,
                ptr::null_mut(),
                false,
                ptr::null(),
                Some(Self::on_get_time_ms),
                ptr::null_mut(),
                match player {
                    Some(p) => p.move_to_ptr(),
                    None => ptr::null_mut(),
                },
                Some(Self::on_playback),
                None,
                user.cast(),
                None,
                ptr::null_mut(),
            )
        };
        pe
    }

    pub fn render_width(&self) -> usize {
        self.render_width
    }

    pub fn render_height(&self) -> usize {
        self.render_height
    }

    pub fn set_reveal_censored(&self, reveal_censored: bool) {
        unsafe { DP_paint_engine_reveal_censored_set(self.paint_engine, reveal_censored) }
    }

    extern "C" fn on_renderer_tile(
        user: *mut c_void,
        tile_x: c_int,
        tile_y: c_int,
        pixels: *mut DP_Pixel8,
    ) {
        let pe = unsafe { user.cast::<Self>().as_mut().unwrap_unchecked() };
        let from_x = tile_x as usize * Self::TILE_SIZE;
        let from_y = tile_y as usize * Self::TILE_SIZE;
        let to_x = (from_x + Self::TILE_SIZE).min(pe.render_width);
        let to_y = (from_y + Self::TILE_SIZE).min(pe.render_height);
        let width = to_x - from_x;
        let height = to_y - from_y;
        for i in 0..height {
            let src = i * Self::TILE_SIZE;
            let dst = (from_y + i) * pe.render_width + from_x;
            unsafe {
                ptr::copy_nonoverlapping(
                    pixels.cast::<u32>().add(src),
                    pe.render_image.as_mut_ptr().add(dst),
                    width,
                );
            }
        }
    }

    extern "C" fn on_renderer_unlock(user: *mut c_void) {
        let pe = unsafe { user.cast::<Self>().as_mut().unwrap_unchecked() };
        pe.render_barrier.wait();
    }

    extern "C" fn on_renderer_resize(
        user: *mut c_void,
        width: c_int,
        height: c_int,
        _prev_width: c_int,
        _prev_height: c_int,
        _offset_x: c_int,
        _offset_y: c_int,
    ) {
        let pe = unsafe { user.cast::<Self>().as_mut().unwrap_unchecked() };
        let w = width as usize;
        let h = height as usize;
        pe.render_width = w;
        pe.render_height = h;
        pe.render_image.resize(w * h, 0);
    }

    extern "C" fn on_save_point(
        _user: *mut c_void,
        _cs: *mut DP_CanvasState,
        _snapshot_requested: bool,
    ) {
    }

    extern "C" fn on_get_time_ms(_user: *mut c_void) -> c_longlong {
        match SystemTime::now().duration_since(UNIX_EPOCH) {
            Ok(d) => d.as_millis() as i64,
            Err(_) => 0,
        }
    }

    extern "C" fn on_playback(user: *mut c_void, position: c_longlong) {
        let pe = unsafe { user.cast::<Self>().as_mut().unwrap_unchecked() };
        pe.playback_channel.0.send(position).unwrap();
    }

    pub fn begin_playback(&mut self) -> Result<()> {
        Self::check_player_result(unsafe { DP_paint_engine_playback_begin(self.paint_engine) })
    }

    pub fn step_playback(&mut self, steps: i64) -> Result<i64> {
        self.playback(None, |paint_engine, user| unsafe {
            DP_paint_engine_playback_step(paint_engine, steps, Some(Self::on_push_message), user)
        })
    }

    pub fn skip_playback(&mut self, steps: i64) -> Result<i64> {
        self.playback(None, |paint_engine, user| unsafe {
            DP_paint_engine_playback_skip_by(
                paint_engine,
                ptr::null_mut(),
                steps,
                false,
                Some(Self::on_push_message),
                user,
            )
        })
    }

    pub fn play_playback_timelapse(
        &mut self,
        msecs: i64,
        crop: Option<PlaybackCrop>,
    ) -> Result<i64> {
        self.playback(crop, |paint_engine, user| unsafe {
            DP_paint_engine_playback_play(
                paint_engine,
                msecs,
                Some(Self::on_filter_timelapse_message),
                Some(Self::on_push_message),
                user,
            )
        })
    }

    fn playback<F>(&mut self, crop: Option<PlaybackCrop>, func: F) -> Result<i64>
    where
        F: FnOnce(*mut DP_PaintEngine, *mut c_void) -> DP_PlayerResult,
    {
        let mut params = PlaybackParams {
            messages: Vec::new(),
            crop,
        };
        let user: *mut PlaybackParams = &mut params;
        Self::check_player_result(func(self.paint_engine, user.cast()))?;
        self.handle(false, true, &mut params.messages);
        Ok(self.playback_channel.1.recv().unwrap())
    }

    pub fn handle(
        &mut self,
        local: bool,
        override_acls: bool,
        messages: &mut Vec<Message>,
    ) -> c_int {
        let user: *mut Self = self;
        unsafe {
            DP_paint_engine_handle_inc(
                self.paint_engine,
                local,
                override_acls,
                messages.len() as c_int,
                messages.as_mut_ptr().cast(),
                Some(Self::on_acls_changed),
                Some(Self::on_laser_trail),
                Some(Self::on_move_pointer),
                user.cast(),
            )
        }
    }

    extern "C" fn on_filter_timelapse_message(user: *mut c_void, msg: *mut DP_Message) -> c_uint {
        let msg_type = unsafe { DP_message_type(msg) };
        if msg_type == DP_MSG_INTERVAL || msg_type == DP_MSG_UNDO {
            // Don't delay timelapse from dead air or undo/redo.
            DP_PAINT_ENGINE_FILTER_MESSAGE_FLAG_NO_TIME
        } else {
            let params = unsafe { user.cast::<PlaybackParams>().as_mut().unwrap_unchecked() };
            if let Some((x, y, width, height)) = params.crop {
                let aa = unsafe { DP_affected_area_make(msg, null_mut()) };
                if !unsafe {
                    DP_affected_area_in_bounds(
                        &aa,
                        x as c_int,
                        y as c_int,
                        width as c_int,
                        height as c_int,
                    )
                } {
                    // Don't delay timelapses from stuff happening outside of the cropped area.
                    return DP_PAINT_ENGINE_FILTER_MESSAGE_FLAG_NO_TIME;
                }
            };
            0
        }
    }

    extern "C" fn on_push_message(user: *mut c_void, msg: *mut DP_Message) {
        let params = unsafe { user.cast::<PlaybackParams>().as_mut().unwrap_unchecked() };
        params.messages.push(Message::new_noinc(msg));
    }

    extern "C" fn on_acls_changed(_user: *mut c_void, _acl_change_flags: c_int) {}

    extern "C" fn on_laser_trail(
        _user: *mut c_void,
        _context_id: c_uint,
        _persistence: c_int,
        _color: u32,
    ) {
    }

    extern "C" fn on_move_pointer(_user: *mut c_void, _context_id: c_uint, _x: c_int, _y: c_int) {}

    pub fn render(&mut self) {
        let user: *mut Self = self;
        let tile_bounds = DP_Rect {
            x1: 0,
            y1: 0,
            x2: c_int::from(u16::MAX),
            y2: c_int::from(u16::MAX),
        };
        unsafe {
            DP_paint_engine_tick(
                self.paint_engine,
                tile_bounds,
                false,
                Some(Self::on_catchup),
                Some(Self::on_reset_lock_changed),
                Some(Self::on_recorder_state_changed),
                Some(Self::on_layer_props_changed),
                Some(Self::on_annotations_changed),
                Some(Self::on_document_metadata_changed),
                Some(Self::on_timeline_changed),
                Some(Self::on_selections_changed),
                Some(Self::on_cursor_moved),
                Some(Self::on_default_layer_set),
                Some(Self::on_undo_depth_limit_set),
                Some(Self::on_censored_layer_revealed),
                user.cast(),
            );
            DP_paint_engine_render_everything(self.paint_engine);
        }
        self.render_barrier.wait();
    }

    extern "C" fn on_catchup(_user: *mut c_void, _progress: c_int) {}
    extern "C" fn on_reset_lock_changed(_user: *mut c_void, _locked: bool) {}
    extern "C" fn on_recorder_state_changed(_user: *mut c_void, _started: bool) {}
    extern "C" fn on_layer_props_changed(_user: *mut c_void, _lpl: *mut DP_LayerPropsList) {}
    extern "C" fn on_annotations_changed(_user: *mut c_void, _al: *mut DP_AnnotationList) {}
    extern "C" fn on_document_metadata_changed(_user: *mut c_void, _dm: *mut DP_DocumentMetadata) {}
    extern "C" fn on_timeline_changed(_user: *mut c_void, _tl: *mut DP_Timeline) {}
    extern "C" fn on_selections_changed(_user: *mut c_void, _ss_or_null: *mut DP_SelectionSet) {}
    extern "C" fn on_cursor_moved(
        _user: *mut c_void,
        _flags: c_uint,
        _context_id: c_uint,
        _layer_id: c_int,
        _x: c_int,
        _y: c_int,
    ) {
    }
    extern "C" fn on_default_layer_set(_user: *mut c_void, _layer_id: c_int) {}
    extern "C" fn on_undo_depth_limit_set(_user: *mut c_void, _undo_depth_limit: c_int) {}
    extern "C" fn on_censored_layer_revealed(_user: *mut c_void, _layer_id: c_int) {}

    pub fn save(&mut self, path: &str, save_type: DP_SaveImageType) -> Result<()> {
        let cpath = CString::new(path)?;
        let cs = unsafe { DP_paint_engine_view_canvas_state_inc(self.paint_engine) };
        let result = unsafe {
            DP_save(
                cs,
                self.main_dc.as_ptr(),
                save_type,
                cpath.as_ptr(),
                ptr::null(),
                None,
                ptr::null_mut(),
            )
        };
        unsafe { DP_canvas_state_decref(cs) }
        if result == DP_SAVE_RESULT_SUCCESS {
            Ok(())
        } else {
            Err(dp_error_anyhow())
        }
    }

    pub fn to_image(&self) -> Result<Image> {
        Image::new_from_pixels(self.render_width, self.render_height, &self.render_image)
    }

    pub fn to_scaled_image(
        &mut self,
        width: usize,
        height: usize,
        expand: bool,
        interpolation: DP_ImageScaleInterpolation,
    ) -> Result<Image> {
        Image::new_from_pixels_scaled(
            self.render_width,
            self.render_height,
            &self.render_image,
            width,
            height,
            expand,
            interpolation,
            &mut self.main_dc,
        )
    }

    pub fn to_scaled_image_crop(
        &mut self,
        width: usize,
        height: usize,
        expand: bool,
        crop_x: usize,
        crop_y: usize,
        crop_width: usize,
        crop_height: usize,
        interpolation: DP_ImageScaleInterpolation,
    ) -> Result<Image> {
        let img = self
            .to_image()?
            .cropped(crop_x, crop_y, crop_width, crop_height)?;
        Image::new_from_pixels_scaled(
            img.width(),
            img.height(),
            img.pixels(),
            width,
            height,
            expand,
            interpolation,
            &mut self.main_dc,
        )
    }

    fn check_player_result(result: DP_PlayerResult) -> Result<()> {
        if result == DP_PLAYER_SUCCESS || result == DP_PLAYER_RECORDING_END {
            Ok(())
        } else {
            Err(dp_error_anyhow())
        }
    }
}

impl Drop for PaintEngine {
    fn drop(&mut self) {
        unsafe { DP_paint_engine_free_join(self.paint_engine) }
    }
}
