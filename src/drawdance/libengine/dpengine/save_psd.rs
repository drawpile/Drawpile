// SPDX-License-Identifier: GPL-3.0-or-later
use anyhow::Result;
use drawdance::{
    common::Output, dp_error_set, DP_BlendMode, DP_CanvasState, DP_DrawContext, DP_LayerContent,
    DP_LayerList, DP_LayerProps, DP_LayerPropsList, DP_SaveResult, DP_UPixel8,
    DP_canvas_state_background_tile_noinc, DP_canvas_state_height,
    DP_canvas_state_layer_props_noinc, DP_canvas_state_layers_noinc, DP_canvas_state_width,
    DP_channel15_to_8, DP_draw_context_pool_require, DP_free, DP_layer_content_to_upixels8,
    DP_layer_content_to_upixels8_cropped, DP_layer_group_children_noinc,
    DP_layer_list_content_at_noinc, DP_layer_list_group_at_noinc, DP_layer_props_blend_mode,
    DP_layer_props_children_noinc, DP_layer_props_isolated, DP_layer_props_list_at_noinc,
    DP_layer_props_list_count, DP_layer_props_opacity, DP_layer_props_title,
    DP_transient_layer_content_decref, DP_transient_layer_content_new_init, DP_BLEND_MODE_ADD,
    DP_BLEND_MODE_BURN, DP_BLEND_MODE_COLOR, DP_BLEND_MODE_DARKEN, DP_BLEND_MODE_DIVIDE,
    DP_BLEND_MODE_DODGE, DP_BLEND_MODE_HARD_LIGHT, DP_BLEND_MODE_HUE, DP_BLEND_MODE_LIGHTEN,
    DP_BLEND_MODE_LINEAR_BURN, DP_BLEND_MODE_LINEAR_LIGHT, DP_BLEND_MODE_LUMINOSITY,
    DP_BLEND_MODE_MULTIPLY, DP_BLEND_MODE_NORMAL, DP_BLEND_MODE_OVERLAY, DP_BLEND_MODE_SATURATION,
    DP_BLEND_MODE_SCREEN, DP_BLEND_MODE_SOFT_LIGHT, DP_BLEND_MODE_SUBTRACT,
    DP_SAVE_RESULT_INTERNAL_ERROR, DP_SAVE_RESULT_OPEN_ERROR, DP_SAVE_RESULT_SUCCESS,
    DP_SAVE_RESULT_WRITE_ERROR,
};
use std::{
    collections::HashMap,
    ffi::{c_char, c_int, c_void},
    panic::catch_unwind,
    ptr::{null, null_mut},
    slice::{from_raw_parts, from_raw_parts_mut},
};

#[derive(Debug, Copy, Clone, PartialEq)]
enum Section {
    Other,
    OpenFolder,
    Divider,
}

impl Section {
    fn flags(self) -> u8 {
        match self {
            // "Irrelevant for rendering" flag.
            Self::Divider | Self::OpenFolder => 0x10,
            Self::Other => 0,
        }
    }

    fn needs_lsect_block(self) -> bool {
        match self {
            Self::Divider | Self::OpenFolder => true,
            Self::Other => false,
        }
    }

    fn value(self) -> u8 {
        match self {
            Self::Other => 0,
            Self::OpenFolder => 1,
            Self::Divider => 3,
        }
    }
}

fn write_preliminary_size_prefix(out: &mut Output) -> Result<usize> {
    let start = out.tell()?;
    out.write_bytes(&[0, 0, 0, 0])?;
    Ok(start)
}

fn write_size_prefix(out: &mut Output, start: usize, alignment: usize) -> Result<()> {
    let end = out.tell()?;
    let actual_end = if alignment == 0 {
        end
    } else {
        let mask = alignment - 1;
        let aligned_end = (end + mask) & !mask;
        for _ in end..aligned_end {
            out.write_bytes(&[0])?;
        }
        aligned_end
    };
    out.seek(start)?;
    out.write_u32_be(u32::try_from(actual_end - start - 4)?)?;
    out.seek(actual_end)?;
    Ok(())
}

fn count_layers_recursive(lpl: *mut DP_LayerPropsList) -> usize {
    let count = unsafe { DP_layer_props_list_count(lpl) };
    let mut total = count as usize;
    for i in 0..count {
        let lp = unsafe { DP_layer_props_list_at_noinc(lpl, i) };
        let child_lpl = unsafe { DP_layer_props_children_noinc(lp) };
        if !child_lpl.is_null() {
            // PSD needs two layers for every group.
            total += 1 + count_layers_recursive(child_lpl);
        }
    }
    return total;
}

fn blend_mode_to_psd(blend_mode: DP_BlendMode) -> &'static [u8] {
    match blend_mode {
        DP_BLEND_MODE_DARKEN => b"dark",
        DP_BLEND_MODE_MULTIPLY => b"mul ",
        DP_BLEND_MODE_BURN => b"idiv",
        DP_BLEND_MODE_LINEAR_BURN => b"lbrn",
        DP_BLEND_MODE_LIGHTEN => b"lite",
        DP_BLEND_MODE_SCREEN => b"scrn",
        DP_BLEND_MODE_DODGE => b"div ",
        DP_BLEND_MODE_ADD => b"lddg",
        DP_BLEND_MODE_OVERLAY => b"over",
        DP_BLEND_MODE_SOFT_LIGHT => b"sLit",
        DP_BLEND_MODE_HARD_LIGHT => b"hLit",
        DP_BLEND_MODE_LINEAR_LIGHT => b"lLit",
        DP_BLEND_MODE_SUBTRACT => b"fsub",
        DP_BLEND_MODE_DIVIDE => b"fdiv",
        DP_BLEND_MODE_HUE => b"hue ",
        DP_BLEND_MODE_SATURATION => b"sat ",
        DP_BLEND_MODE_COLOR => b"colr",
        DP_BLEND_MODE_LUMINOSITY => b"lum ",
        _ => b"norm",
    }
}

fn write_layer_info(
    out: &mut Output,
    blend_mode: DP_BlendMode,
    opacity: u8,
    title: &[u8],
    section: Section,
    isolated: bool,
) -> Result<()> {
    // We write placeholders for an empty layer. If the layer has content, it
    // will be overwritten later. Channel size is 2, since they have a header.
    out.write_bytes(&[
        0, 0, 0, 0, 0, 0, 0, 0, // Top, left.
        0, 0, 0, 0, 0, 0, 0, 0, // Bottom, right.
        0, 4, // Channel count, 4 for ARGB.
        255, 255, 0, 0, 0, 2, // Alpha channel, id -1, size 2.
        0, 0, 0, 0, 0, 2, // Red channel, id 0, size 2.
        0, 1, 0, 0, 0, 2, // Blue channel, id 1, size 2.
        0, 2, 0, 0, 0, 2, // Green channel id 2, size 2.
        56, 66, 73, 77, // "8BIM" magic number.
    ])?;

    // Blend mode.
    let psd_blend_mode = blend_mode_to_psd(blend_mode);
    out.write_bytes(psd_blend_mode)?;

    // Opacity, clipping (none), flags, reserved byte.
    out.write_bytes(&[opacity, 0, section.flags(), 0])?;

    // Extra layer data section.
    let section_start = write_preliminary_size_prefix(out)?;
    out.write_bytes(&[
        0, 0, 0, 0, // Layer mask (none).
        0, 0, 0, 0, // Layer blending ranges (none).
    ])?;

    let len = title.len().min(255);
    out.write_bytes(&[len as u8])?;
    if title.is_ascii() {
        out.write_bytes(&title[0..len])?;
    } else {
        let ascii_title: Vec<u8> = title[0..len]
            .iter()
            .map(|b| if b.is_ascii() { *b } else { 0x5fu8 }) // Underscore.
            .collect();
        out.write_bytes(&ascii_title)?;
    }
    match (len + 1) % 4 {
        1 => out.write_bytes(&[0, 0, 0])?,
        2 => out.write_bytes(&[0, 0])?,
        3 => out.write_bytes(&[0])?,
        _ => {}
    }

    if section.needs_lsect_block() {
        let value = section.value();
        out.write_bytes(&[
            56, 66, 73, 77, // "8BIM" magic number.
            108, 115, 99, 116, // "lsct" block key.
            0, 0, 0, 12, // Block content size.
            0, 0, 0, value, // Section type.
            56, 66, 73, 77, // "8BIM" magic number again.
        ])?;
        out.write_bytes(if isolated { psd_blend_mode } else { b"pass" })?;
    }

    write_size_prefix(out, section_start, 0)?;
    Ok(())
}

fn write_layer_props_info(
    lp: *mut DP_LayerProps,
    out: &mut Output,
    section: Section,
    isolated: bool,
) -> Result<()> {
    let blend_mode = unsafe { DP_layer_props_blend_mode(lp) } as DP_BlendMode;
    let opacity = unsafe { DP_channel15_to_8(DP_layer_props_opacity(lp)) };
    if section == Section::Divider {
        write_layer_info(
            out,
            blend_mode,
            opacity,
            b"</Layer group>",
            section,
            isolated,
        )?;
    } else {
        let mut title_length = 0usize;
        let title_data = unsafe { DP_layer_props_title(lp, &mut title_length) };
        let title = unsafe { from_raw_parts(title_data.cast(), title_length) };
        write_layer_info(out, blend_mode, opacity, title, section, isolated)?;
    }
    Ok(())
}

fn write_layer_content_info(
    lp: *mut DP_LayerProps,
    out: &mut Output,
    layer_offsets: &mut HashMap<*mut c_void, usize>,
) -> Result<()> {
    layer_offsets.insert(lp.cast(), out.tell()?);
    write_layer_props_info(lp, out, Section::Other, true)?;
    Ok(())
}

fn write_layer_infos_recursive(
    lpl: *mut DP_LayerPropsList,
    out: &mut Output,
    layer_offsets: &mut HashMap<*mut c_void, usize>,
) -> Result<()> {
    let count = unsafe { DP_layer_props_list_count(lpl) };
    for i in 0..count {
        let lp = unsafe { DP_layer_props_list_at_noinc(lpl, i) };
        let child_lpl = unsafe { DP_layer_props_children_noinc(lp) };
        if child_lpl.is_null() {
            write_layer_content_info(lp, out, layer_offsets)?;
        } else {
            let isolated = unsafe { DP_layer_props_isolated(lp) };
            write_layer_props_info(lp, out, Section::Divider, isolated)?;
            write_layer_infos_recursive(child_lpl, out, layer_offsets)?;
            write_layer_props_info(lp, out, Section::OpenFolder, isolated)?;
        }
    }
    Ok(())
}

fn write_layer_info_section(
    cs: *mut DP_CanvasState,
    out: &mut Output,
    layer_offsets: &mut HashMap<*mut c_void, usize>,
) -> Result<()> {
    let section_start = write_preliminary_size_prefix(out)?;

    // Number of layers, plus one for the background.
    let lpl = unsafe { DP_canvas_state_layer_props_noinc(cs) };
    out.write_i16_be(i16::try_from(count_layers_recursive(lpl) + 1)?)?;

    // Background layer info.
    layer_offsets.insert(null_mut(), out.tell()?);
    write_layer_info(
        out,
        DP_BLEND_MODE_NORMAL,
        255,
        b"Background",
        Section::Other,
        true,
    )?;

    // Remaining layer info.
    write_layer_infos_recursive(lpl, out, layer_offsets)?;

    write_size_prefix(out, section_start, 2)?;
    Ok(())
}

fn extract_channel(pixels: &[DP_UPixel8], src: &mut [u8], extract: fn(DP_UPixel8) -> u8) {
    assert_eq!(pixels.len(), src.len());
    for i in 0..pixels.len() {
        *unsafe { src.get_unchecked_mut(i) } = extract(unsafe { *pixels.get_unchecked(i) });
    }
}

fn clear_buffer(dst: &mut [u8]) {
    for d in dst {
        *d = 0;
    }
}

// SPDX-SnippetBegin
// SPDX-License-Identifier: GPL-2.0-or-later
// SDPX—SnippetName: PSD RLE compression from Krita, originally from GIMP
fn rle_compress(src: &[u8], dst: &mut [u8]) -> Result<usize> {
    let mut remaining = i32::try_from(src.len())?;
    let mut length = 0;
    let mut start = 0;
    let mut dst_ptr = 0;
    clear_buffer(dst);

    while remaining > 0 {
        // Look for characters matching the first.
        let mut i = 0;
        while i < 128 && remaining - i > 0 && src[start] == src[start + i as usize] {
            i += 1;
        }

        if i > 1 {
            // Match found.
            dst[dst_ptr] = -(i - 1) as u8;
            dst[dst_ptr + 1] = src[start];
            dst_ptr += 2;
            start += i as usize;
            remaining -= i;
            length += 2;
        } else {
            // Look for characters different from the previous.
            i = 0;
            while (i < 128)
                && (remaining - (i + 1) > 0)
                && (src[start + i as usize] != src[start + i as usize + 1]
                    || remaining - (i + 2) <= 0
                    || src[start + i as usize] != src[start + i as usize + 2])
            {
                i += 1;
            }

            // If there's only 1 remaining, the previous WHILE stmt doesn't catch it
            if remaining == 1 {
                i = 1;
            }

            if i > 0 {
                dst[dst_ptr] = (i - 1) as u8;
                let si = i as usize;
                for j in 0..si {
                    dst[dst_ptr + 1 + j] = src[start + j];
                }
                dst_ptr += 1 + si;
                start += si;
                remaining -= i;
                length += i + 1;
            }
        }
    }

    Ok(length as usize)
}
// SPDX-SnippetEnd

fn write_channel(
    out: &mut Output,
    rows: usize,
    stride: usize,
    pixels: &[DP_UPixel8],
    counts: &mut [u8],
    src: &mut [u8],
    dst: &mut [u8],
    extract: fn(DP_UPixel8) -> u8,
) -> Result<usize> {
    let start_pos = out.tell()?;
    out.write_bytes(&[0, 1])?;
    clear_buffer(counts);
    out.write_bytes(counts)?;

    for i in 0..rows {
        let offset = i * stride;
        extract_channel(&pixels[offset..offset + stride], src, extract);

        let length = rle_compress(&src, dst)?;
        out.write_bytes(&dst[0..length])?;

        let count_bytes = u16::try_from(length)?.to_be_bytes();
        counts[i * 2] = count_bytes[0];
        counts[i * 2 + 1] = count_bytes[1];
    }

    let end_pos = out.tell()?;
    out.seek(start_pos + 2)?;
    out.write_bytes(counts)?;
    out.seek(end_pos)?;
    Ok(end_pos - start_pos)
}

fn write_pixel_channels(
    dc: *mut DP_DrawContext,
    out: &mut Output,
    rows: usize,
    stride: usize,
    pixels: &[DP_UPixel8],
) -> Result<(usize, usize, usize, usize)> {
    let counts_length = rows * 2;
    let pool_size = counts_length + stride * 3;
    let pool: &mut [u8] = unsafe {
        from_raw_parts_mut(
            DP_draw_context_pool_require(dc, pool_size).cast(),
            pool_size,
        )
    };
    let (counts, buffers) = pool.split_at_mut(counts_length);
    let (src, dst) = buffers.split_at_mut(stride);
    let a = write_channel(out, rows, stride, pixels, counts, src, dst, DP_UPixel8::a)?;
    let r = write_channel(out, rows, stride, pixels, counts, src, dst, DP_UPixel8::r)?;
    let g = write_channel(out, rows, stride, pixels, counts, src, dst, DP_UPixel8::g)?;
    let b = write_channel(out, rows, stride, pixels, counts, src, dst, DP_UPixel8::b)?;
    Ok((a, r, g, b))
}

fn write_pixel_data(
    dc: *mut DP_DrawContext,
    out: &mut Output,
    offset: usize,
    pixels: *const DP_UPixel8,
    offset_x: c_int,
    offset_y: c_int,
    width: c_int,
    height: c_int,
) -> Result<()> {
    if pixels.is_null() || width <= 0 || height <= 0 {
        // Empty layer or group. Just write blank pixel data.
        out.write_bytes(&[0, 0, 0, 0, 0, 0, 0, 0])?;
    } else {
        // We got some pixel data, run-length encode it. That's the default in
        // Photoshop apparently and Krita also always uses this option.
        let rows = usize::try_from(height)?;
        let stride = usize::try_from(width)?;
        let (a, r, g, b) = write_pixel_channels(dc, out, rows, stride, unsafe {
            from_raw_parts(pixels, rows * stride)
        })?;
        // Go back and fill in the channel size information.
        let pos = out.tell()?;
        out.seek(offset)?;
        // Bounding rectangle.
        let top = u32::try_from(offset_y)?;
        out.write_u32_be(top)?;
        let left = u32::try_from(offset_x)?;
        out.write_u32_be(left)?;
        let h = u32::try_from(height)?;
        out.write_u32_be(top + h)?;
        let w = u32::try_from(width)?;
        out.write_u32_be(left + w)?;
        // Number of channels, always 4 for ARGB.
        out.write_bytes(&[0, 4])?;
        // Channel ids and the sizes of their pixel data.
        out.write_i16_be(-1)?;
        out.write_u32_be(u32::try_from(a)?)?;
        out.write_i16_be(0)?;
        out.write_u32_be(u32::try_from(r)?)?;
        out.write_i16_be(1)?;
        out.write_u32_be(u32::try_from(g)?)?;
        out.write_i16_be(2)?;
        out.write_u32_be(u32::try_from(b)?)?;
        out.seek(pos)?;
    }
    Ok(())
}

fn write_background_pixel_data(
    cs: *mut DP_CanvasState,
    dc: *mut DP_DrawContext,
    out: &mut Output,
    layer_offsets: &HashMap<*mut c_void, usize>,
) -> Result<()> {
    let width = unsafe { DP_canvas_state_width(cs) };
    let height = unsafe { DP_canvas_state_height(cs) };
    let pixels = unsafe {
        let tlc = DP_transient_layer_content_new_init(
            DP_canvas_state_width(cs),
            DP_canvas_state_height(cs),
            DP_canvas_state_background_tile_noinc(cs),
        );
        let px = DP_layer_content_to_upixels8(tlc.cast(), 0, 0, width, height);
        DP_transient_layer_content_decref(tlc);
        px
    };
    let result = write_pixel_data(
        dc,
        out,
        *layer_offsets.get(&null_mut()).unwrap(),
        pixels,
        0,
        0,
        width,
        height,
    );
    unsafe { DP_free(pixels.cast()) }
    result
}

fn write_layer_content_pixel_data(
    lc: *mut DP_LayerContent,
    dc: *mut DP_DrawContext,
    out: &mut Output,
    offset: usize,
) -> Result<()> {
    let mut offset_x: c_int = 0;
    let mut offset_y: c_int = 0;
    let mut width: c_int = 0;
    let mut height: c_int = 0;
    let pixels = unsafe {
        DP_layer_content_to_upixels8_cropped(
            lc,
            &mut offset_x,
            &mut offset_y,
            &mut width,
            &mut height,
        )
    };
    let result = write_pixel_data(dc, out, offset, pixels, offset_x, offset_y, width, height);
    unsafe { DP_free(pixels.cast()) }
    result
}

fn write_layer_pixel_data_recursive(
    ll: *mut DP_LayerList,
    lpl: *mut DP_LayerPropsList,
    dc: *mut DP_DrawContext,
    out: &mut Output,
    layer_offsets: &HashMap<*mut c_void, usize>,
) -> Result<()> {
    let count = unsafe { DP_layer_props_list_count(lpl) };
    for i in 0..count {
        let lp = unsafe { DP_layer_props_list_at_noinc(lpl, i) };
        let child_lpl = unsafe { DP_layer_props_children_noinc(lp) };
        if child_lpl.is_null() {
            let lc = unsafe { DP_layer_list_content_at_noinc(ll, i) };
            let offset = *layer_offsets.get(&lp.cast()).unwrap();
            write_layer_content_pixel_data(lc, dc, out, offset)?;
        } else {
            write_pixel_data(dc, out, 0, null(), 0, 0, 0, 0)?;
            let lg = unsafe { DP_layer_list_group_at_noinc(ll, i) };
            let child_ll = unsafe { DP_layer_group_children_noinc(lg) };
            write_layer_pixel_data_recursive(child_ll, child_lpl, dc, out, layer_offsets)?;
            write_pixel_data(dc, out, 0, null(), 0, 0, 0, 0)?;
        }
    }
    Ok(())
}

fn write_layer_pixel_data_section(
    cs: *mut DP_CanvasState,
    dc: *mut DP_DrawContext,
    out: &mut Output,
    layer_offsets: &HashMap<*mut c_void, usize>,
) -> Result<()> {
    write_background_pixel_data(cs, dc, out, layer_offsets)?;
    let ll = unsafe { DP_canvas_state_layers_noinc(cs) };
    let lpl = unsafe { DP_canvas_state_layer_props_noinc(cs) };
    write_layer_pixel_data_recursive(ll, lpl, dc, out, layer_offsets)?;
    Ok(())
}

fn write_layer_sections(
    cs: *mut DP_CanvasState,
    dc: *mut DP_DrawContext,
    mut out: Output,
) -> Result<()> {
    let section_start = write_preliminary_size_prefix(&mut out)?;
    let mut layer_offsets = HashMap::new();
    write_layer_info_section(cs, &mut out, &mut layer_offsets)?;
    write_layer_pixel_data_section(cs, dc, &mut out, &layer_offsets)?;
    out.write_bytes(&[0, 0, 0, 0])?; // Global layer mask info (none).
    write_size_prefix(&mut out, section_start, 2)?;
    out.close()?;
    Ok(())
}

fn write_psd(cs: *mut DP_CanvasState, dc: *mut DP_DrawContext, mut out: Output) -> Result<()> {
    // Magic "8BPS", 2 bytes for the version (0, 1), 6 reserved zero bytes.
    out.write_bytes(&[
        56, 66, 80, 83, // "8BPS" magic number.
        0, 1, // Version, always 1.
        0, 0, 0, 0, 0, 0, // Reserved, always 0.
        0, 4, // Number of channels, we always use RGBA, so it's 4.
    ])?;
    // Dimensions, height first for some reason. PSD supports at most 30,000
    // pixels in either direction, which is more than the u16 range we allow.
    out.write_u32_be(u32::try_from(unsafe { DP_canvas_state_height(cs) })?)?;
    out.write_u32_be(u32::try_from(unsafe { DP_canvas_state_width(cs) })?)?;
    out.write_bytes(&[
        0, 8, // Channel depth, we use 8 bit channels.
        0, 3, // Color mode, we always use 3 for RGB.
        0, 0, 0, 0, // Color mode section length, always zero for RGB.
        0, 0, 0, 0, // Image resources section length, we don't have any.
    ])?;
    write_layer_sections(cs, dc, out)?;
    Ok(())
}

fn save_psd(
    cs: *mut DP_CanvasState,
    path: *const c_char,
    dc: *mut DP_DrawContext,
) -> DP_SaveResult {
    let out = match Output::new_from_path(path) {
        Ok(o) => o,
        Err(_) => return DP_SAVE_RESULT_OPEN_ERROR,
    };
    match write_psd(cs, dc, out) {
        Ok(()) => DP_SAVE_RESULT_SUCCESS,
        Err(_) => DP_SAVE_RESULT_WRITE_ERROR,
    }
}

#[no_mangle]
pub extern "C" fn DP_save_psd(
    cs: *mut DP_CanvasState,
    path: *const c_char,
    dc: *mut DP_DrawContext,
) -> DP_SaveResult {
    match catch_unwind(|| save_psd(cs, path, dc)) {
        Ok(save_result) => save_result,
        Err(_) => {
            dp_error_set("Panic while saving PSD");
            DP_SAVE_RESULT_INTERNAL_ERROR
        }
    }
}
