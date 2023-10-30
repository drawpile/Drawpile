// SPDX-License-Identifier: GPL-3.0-or-later
use drawdance::{
    engine::{
        AttachedLayerPropsList, BaseCanvasState, BaseLayerList, BaseLayerProps, BaseLayerPropsList,
        BaseTransientCanvasState, BaseTransientDocumentMetadata, BaseTransientLayerList,
        BaseTransientLayerProps, BaseTransientLayerPropsList, BaseTransientTimeline,
        BaseTransientTrack, CanvasState, TransientCanvasState, TransientKeyFrame,
        TransientLayerGroup, TransientLayerList, TransientLayerProps, TransientLayerPropsList,
        TransientTimeline, TransientTrack,
    },
    DP_CanvasState, DP_DrawContext, DP_LoadResult, DP_TransientLayerProps, DP_TransientTrack,
    DP_load_ora,
};
use std::{
    collections::HashSet,
    ffi::{c_char, c_int, c_void},
    ptr::null_mut,
};

pub type LoadOldAnimationSetGroupTitleFn =
    unsafe extern "C" fn(tlp: *mut DP_TransientLayerProps, i: c_int);

pub type LoadOldAnimationSetTrackTitleFn =
    unsafe extern "C" fn(tt: *mut DP_TransientTrack, i: c_int);

extern "C" fn on_fixed_layer(user: *mut c_void, layer_id: c_int) {
    let fixed_layers: &mut HashSet<c_int> = unsafe { &mut *user.cast() };
    fixed_layers.insert(layer_id);
}

fn load_ora(
    dc: *mut DP_DrawContext,
    path: *const c_char,
    out_result: *mut DP_LoadResult,
) -> (Option<CanvasState>, HashSet<c_int>) {
    let mut fixed_layer_ids = HashSet::new();
    let fixed_layer_ids_ptr: *mut HashSet<c_int> = &mut fixed_layer_ids;
    let cs = CanvasState::new_noinc_nullable(unsafe {
        DP_load_ora(
            dc,
            path,
            Some(on_fixed_layer),
            fixed_layer_ids_ptr.cast(),
            out_result,
        )
    });
    (cs, fixed_layer_ids)
}

fn count_elements(
    fixed_layer_ids: &HashSet<c_int>,
    lpl: &AttachedLayerPropsList<CanvasState>,
) -> (c_int, c_int) {
    let mut track_count = 0;
    let mut key_frame_count = 0;
    let mut in_frame_run = false;
    for lp in lpl.iter() {
        if fixed_layer_ids.contains(&lp.id()) {
            track_count += 1;
            in_frame_run = false;
        } else {
            key_frame_count += 1;
            if !in_frame_run {
                track_count += 1;
                in_frame_run = true;
            }
        }
    }
    (track_count, key_frame_count)
}

fn count_frame_run(
    fixed_layer_ids: &HashSet<c_int>,
    lpl: &AttachedLayerPropsList<CanvasState>,
    layer_count: c_int,
    start_index: c_int,
) -> c_int {
    let mut end_index = start_index;
    while end_index < layer_count {
        if fixed_layer_ids.contains(&lpl.at(end_index).id()) {
            break;
        }
        end_index += 1;
    }
    end_index - start_index
}

fn convert_animation(
    cs: CanvasState,
    dc: *mut DP_DrawContext,
    hold_time: c_int,
    framerate: c_int,
    set_group_title: LoadOldAnimationSetGroupTitleFn,
    set_track_title: LoadOldAnimationSetTrackTitleFn,
    fixed_layer_ids: HashSet<c_int>,
) -> *mut DP_CanvasState {
    let width = cs.width();
    let height = cs.height();
    let ll = cs.layers();
    let lpl = cs.layer_props();
    let layer_count = lpl.count();
    let (track_count, key_frame_count) = count_elements(&fixed_layer_ids, &lpl);

    let mut tll = TransientLayerList::new_init(track_count);
    let mut tlpl = TransientLayerPropsList::new_init(track_count);
    let mut ttl = TransientTimeline::new(track_count);

    let mut layer_index = 0;
    let mut group_index = 0;
    let mut track_index = 0;
    let mut key_frame_index = 0;
    let mut next_layer_id = 0x100;
    let mut next_track_id = 0x100;
    while layer_index < layer_count {
        // Count how many frames there are until the end or the next fixed layer.
        let frame_run = count_frame_run(&fixed_layer_ids, &lpl, layer_count, layer_index);
        // No frames in the run means this must be a fixed layer.
        if frame_run == 0 {
            tll.set_at_inc(&ll.at(layer_index), track_index);

            let lp = lpl.at(layer_index);
            let mut tlp = TransientLayerProps::new(&lp);
            tlp.set_id(next_layer_id);
            tlpl.set_transient_at_noinc(tlp, track_index);

            let mut tt = TransientTrack::new_init(1);
            tt.set_id(next_track_id);
            tt.set_title(lp.title());
            tt.set_transient_at_noinc(0, TransientKeyFrame::new_init(next_layer_id, 0), 0);
            ttl.set_transient_at_noinc(tt, track_index);

            layer_index += 1;
        } else {
            let mut child_tll = TransientLayerList::new_init(frame_run);
            let mut child_tlpl = TransientLayerPropsList::new_init(frame_run);

            let needs_blank_frame = key_frame_index + frame_run < key_frame_count;
            let mut tt = TransientTrack::new_init(frame_run + c_int::from(needs_blank_frame));
            tt.set_id(next_track_id);
            unsafe { set_track_title(tt.transient_ptr(), group_index) };

            for i in 0..frame_run {
                child_tll.set_at_inc(&ll.at(layer_index), i);

                let mut child_tlp = TransientLayerProps::new(&lpl.at(layer_index));
                child_tlp.set_id(next_layer_id);
                child_tlpl.set_transient_at_noinc(child_tlp, i);

                tt.set_transient_at_noinc(
                    key_frame_index * hold_time,
                    TransientKeyFrame::new_init(next_layer_id, 0),
                    i,
                );

                layer_index += 1;
                key_frame_index += 1;
                next_layer_id += 1;
            }

            let tlg = TransientLayerGroup::new_init_with_transient_children_noinc(
                width, height, child_tll,
            );
            tll.set_transient_group_at_noinc(tlg, track_index);

            let mut tlp = TransientLayerProps::new_init_with_transient_children_noinc(
                next_layer_id,
                child_tlpl,
            );
            tlp.set_isolated(false);
            unsafe { set_group_title(tlp.transient_ptr(), group_index) };
            tlpl.set_transient_at_noinc(tlp, track_index);

            if needs_blank_frame {
                tt.set_transient_at_noinc(
                    key_frame_index * hold_time,
                    TransientKeyFrame::new_init(0, 0),
                    frame_run,
                );
            }
            ttl.set_transient_at_noinc(tt, track_index);

            group_index += 1;
        }
        track_index += 1;
        next_track_id += 1;
        next_layer_id += 1;
    }

    let mut tcs = TransientCanvasState::new(&cs);
    let mut tdm = tcs.transient_metadata();
    tdm.set_framerate(framerate);
    tdm.set_frame_count(1.max(key_frame_count * hold_time));
    tcs.set_transient_layers_noinc(tll);
    tcs.set_transient_layer_props_noinc(tlpl);
    tcs.set_transient_timeline_noinc(ttl);
    tcs.reindex_layer_routes(dc);
    tcs.persist().leak_persistent()
}

#[no_mangle]
pub extern "C" fn DP_load_old_animation(
    dc: *mut DP_DrawContext,
    path: *const c_char,
    hold_time: c_int,
    framerate: c_int,
    set_group_title: LoadOldAnimationSetGroupTitleFn,
    set_track_title: LoadOldAnimationSetTrackTitleFn,
    out_result: *mut DP_LoadResult,
) -> *mut DP_CanvasState {
    match load_ora(dc, path, out_result) {
        (Some(cs), fixed_layer_ids) => convert_animation(
            cs,
            dc,
            hold_time,
            framerate,
            set_group_title,
            set_track_title,
            fixed_layer_ids,
        ),
        (None, _) => null_mut(),
    }
}
