// SPDX-License-Identifier: GPL-3.0-or-later
use anyhow::Result;
use drawdance::{
    common::Output,
    dp_error_set,
    engine::{
        BaseCanvasState, BaseLayerContent, BaseLayerGroup, BaseLayerList, BaseLayerProps,
        BaseLayerPropsList, CanvasState, LayerContent, LayerList, LayerProps, LayerPropsList,
        TransientLayerContent,
    },
    DP_BlendMode, DP_CanvasState, DP_DrawContext, DP_SaveResult, DP_UPixel8,
    DP_draw_context_pool_require, DP_BLEND_MODE_ADD, DP_BLEND_MODE_BURN, DP_BLEND_MODE_COLOR,
    DP_BLEND_MODE_DARKEN, DP_BLEND_MODE_DIVIDE, DP_BLEND_MODE_DODGE, DP_BLEND_MODE_HARD_LIGHT,
    DP_BLEND_MODE_HUE, DP_BLEND_MODE_LIGHTEN, DP_BLEND_MODE_LINEAR_BURN,
    DP_BLEND_MODE_LINEAR_LIGHT, DP_BLEND_MODE_LUMINOSITY, DP_BLEND_MODE_MULTIPLY,
    DP_BLEND_MODE_NORMAL, DP_BLEND_MODE_OVERLAY, DP_BLEND_MODE_SATURATION, DP_BLEND_MODE_SCREEN,
    DP_BLEND_MODE_SOFT_LIGHT, DP_BLEND_MODE_SUBTRACT, DP_SAVE_RESULT_BAD_ARGUMENTS,
    DP_SAVE_RESULT_INTERNAL_ERROR, DP_SAVE_RESULT_OPEN_ERROR, DP_SAVE_RESULT_SUCCESS,
    DP_SAVE_RESULT_WRITE_ERROR,
};
use std::{
    collections::HashMap,
    ffi::{c_char, c_int, c_void},
    panic::catch_unwind,
    ptr::null_mut,
    slice::from_raw_parts_mut,
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

fn count_layers_recursive(lpl: &LayerPropsList) -> usize {
    let count = lpl.count();
    let mut total = count as usize;
    for i in 0..count {
        let lp = lpl.at(i);
        if let Some(child_lpl) = lp.children() {
            // PSD needs two layers for every group.
            total += 1 + count_layers_recursive(&child_lpl);
        }
    }
    total
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

fn write_layer_title(out: &mut Output, title: &[u8]) -> Result<()> {
    let len = title.len().min(255);
    out.write_bytes(&[len as u8])?;
    if title.is_ascii() {
        out.write_bytes(&title[0..len])?;
    } else {
        let ascii_title: Vec<u8> = title[0..len]
            .iter()
            .map(|b| if b.is_ascii() { *b } else { 0x5f_u8 }) // Underscore.
            .collect();
        out.write_bytes(&ascii_title)?;
    }
    match (len + 1) % 4 {
        1 => out.write_bytes(&[0, 0, 0])?,
        2 => out.write_bytes(&[0, 0])?,
        3 => out.write_bytes(&[0])?,
        _ => {}
    }
    Ok(())
}

fn write_layer_utf16be_title(out: &mut Output, title: &[u8]) -> Result<()> {
    out.write_bytes(&[
        56, 66, 73, 77, // "8BIM" magic number.
        108, 117, 110, 105, // "luni" block key.
    ])?;

    let utf16be_title: Vec<u8> = String::from_utf8_lossy(title)
        .encode_utf16()
        .flat_map(u16::to_be_bytes)
        .collect();

    let block_len = 4 + utf16be_title.len() + 2;
    let needs_pad = block_len % 2 != 0;
    out.write_u32_be(u32::try_from(if needs_pad {
        block_len + 1
    } else {
        block_len
    })?)?;

    let utf16be_len = u32::try_from(utf16be_title.len() / 2)?;
    out.write_u32_be(utf16be_len)?; // Length in 16 bit units.
    out.write_bytes(&utf16be_title)?; // Title itself in UTF-16BE.
                                      // Trailing NUL and potentially a pad byte.
    out.write_bytes(if needs_pad { &[0, 0, 0] } else { &[0, 0] })?;
    Ok(())
}

fn write_layer_info(
    out: &mut Output,
    blend_mode: DP_BlendMode,
    opacity: u8,
    hidden: bool,
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
    let flags = if hidden { 0x2 } else { 0x0 };
    out.write_bytes(&[opacity, 0, flags | section.flags(), 0])?;

    // Extra layer data section.
    let section_start = write_preliminary_size_prefix(out)?;
    out.write_bytes(&[
        0, 0, 0, 0, // Layer mask (none).
        0, 0, 0, 0, // Layer blending ranges (none).
    ])?;

    write_layer_title(out, title)?;
    write_layer_utf16be_title(out, title)?;

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
    lp: &LayerProps,
    out: &mut Output,
    section: Section,
    isolated: bool,
) -> Result<()> {
    let blend_mode = lp.blend_mode();
    let opacity = lp.opacity_u8();
    let hidden = lp.hidden();
    if section == Section::Divider {
        write_layer_info(
            out,
            blend_mode,
            opacity,
            hidden,
            b"</Layer group>",
            section,
            isolated,
        )?;
    } else {
        let title = lp.title_u8();
        write_layer_info(out, blend_mode, opacity, hidden, title, section, isolated)?;
    }
    Ok(())
}

fn write_layer_content_info(
    lp: &LayerProps,
    out: &mut Output,
    layer_offsets: &mut HashMap<*mut c_void, usize>,
) -> Result<()> {
    layer_offsets.insert(lp.persistent_ptr().cast(), out.tell()?);
    write_layer_props_info(lp, out, Section::Other, true)?;
    Ok(())
}

fn write_layer_infos_recursive(
    lpl: &LayerPropsList,
    out: &mut Output,
    layer_offsets: &mut HashMap<*mut c_void, usize>,
) -> Result<()> {
    let count = lpl.count();
    for i in 0..count {
        let lp = lpl.at(i);
        if let Some(child_lpl) = lp.children() {
            let isolated = lp.isolated();
            write_layer_props_info(&lp, out, Section::Divider, isolated)?;
            write_layer_infos_recursive(&child_lpl, out, layer_offsets)?;
            write_layer_props_info(&lp, out, Section::OpenFolder, isolated)?;
        } else {
            write_layer_content_info(&lp, out, layer_offsets)?;
        }
    }
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

        let length = rle_compress(src, dst)?;
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
    pixels: &[DP_UPixel8],
    offset_x: c_int,
    offset_y: c_int,
    width: c_int,
    height: c_int,
) -> Result<()> {
    if pixels.is_empty() || width <= 0 || height <= 0 {
        // Empty layer or group. Just write blank pixel data.
        out.write_bytes(&[0, 0, 0, 0, 0, 0, 0, 0])?;
    } else {
        // We got some pixel data, run-length encode it. That's the default in
        // Photoshop apparently and Krita also always uses this option.
        let rows = usize::try_from(height)?;
        let stride = usize::try_from(width)?;
        let (a, r, g, b) = write_pixel_channels(dc, out, rows, stride, pixels)?;
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
    cs: &CanvasState,
    dc: *mut DP_DrawContext,
    out: &mut Output,
    layer_offsets: &HashMap<*mut c_void, usize>,
) -> Result<()> {
    let width = cs.width();
    let height = cs.height();
    let pixels = TransientLayerContent::new_init(width, height, cs.background_tile().as_deref())
        .to_upixels8(0, 0, width, height);
    write_pixel_data(
        dc,
        out,
        *layer_offsets.get(&null_mut()).unwrap(),
        pixels.as_slice(),
        0,
        0,
        width,
        height,
    )
}

fn write_layer_content_pixel_data(
    lc: &LayerContent,
    censored: bool,
    dc: *mut DP_DrawContext,
    out: &mut Output,
    offset: usize,
) -> Result<()> {
    let (pixels, offset_x, offset_y, width, height) = lc.to_upixels8_cropped(censored);
    write_pixel_data(
        dc,
        out,
        offset,
        pixels.as_slice(),
        offset_x,
        offset_y,
        width,
        height,
    )
}

fn write_layer_pixel_data_recursive(
    ll: &LayerList,
    lpl: &LayerPropsList,
    dc: *mut DP_DrawContext,
    out: &mut Output,
    layer_offsets: &HashMap<*mut c_void, usize>,
    parent_censored: bool,
) -> Result<()> {
    let count = lpl.count();
    for i in 0..count {
        let lp = lpl.at(i);
        if let Some(child_lpl) = lp.children() {
            write_pixel_data(dc, out, 0, &[], 0, 0, 0, 0)?;
            let lg = ll.group_at(i);
            write_layer_pixel_data_recursive(
                &lg.children(),
                &child_lpl,
                dc,
                out,
                layer_offsets,
                lp.censored(),
            )?;
            write_pixel_data(dc, out, 0, &[], 0, 0, 0, 0)?;
        } else {
            let lc = ll.content_at(i);
            let offset = *layer_offsets.get(&lp.persistent_ptr().cast()).unwrap();
            write_layer_content_pixel_data(&lc, parent_censored || lp.censored(), dc, out, offset)?;
        }
    }
    Ok(())
}

fn write_layer_pixel_data_section(
    cs: &CanvasState,
    dc: *mut DP_DrawContext,
    out: &mut Output,
    layer_offsets: &HashMap<*mut c_void, usize>,
) -> Result<()> {
    write_background_pixel_data(cs, dc, out, layer_offsets)?;
    let ll = cs.layers();
    let lpl = cs.layer_props();
    write_layer_pixel_data_recursive(&ll, &lpl, dc, out, layer_offsets, false)?;
    Ok(())
}

fn write_layer_info_section(
    cs: &CanvasState,
    dc: *mut DP_DrawContext,
    out: &mut Output,
    layer_offsets: &mut HashMap<*mut c_void, usize>,
) -> Result<()> {
    let section_start = write_preliminary_size_prefix(out)?;

    // Number of layers, plus one for the background. As a negative number
    // because, uh, something about alpha channels. GIMP requires that anyway.
    let lpl = cs.layer_props();
    out.write_i16_be(-i16::try_from(count_layers_recursive(&lpl) + 1)?)?;

    // Background layer info.
    layer_offsets.insert(null_mut(), out.tell()?);
    write_layer_info(
        out,
        DP_BLEND_MODE_NORMAL,
        255,
        false,
        b"Background",
        Section::Other,
        true,
    )?;

    // Remaining layer info.
    write_layer_infos_recursive(&lpl, out, layer_offsets)?;

    // Channel pixel data.
    write_layer_pixel_data_section(cs, dc, out, layer_offsets)?;

    write_size_prefix(out, section_start, 2)?;
    Ok(())
}

fn write_layer_sections(cs: &CanvasState, dc: *mut DP_DrawContext, out: &mut Output) -> Result<()> {
    let section_start = write_preliminary_size_prefix(out)?;
    let mut layer_offsets = HashMap::new();
    write_layer_info_section(cs, dc, out, &mut layer_offsets)?;
    out.write_bytes(&[0, 0, 0, 0])?; // Global layer mask info (none).
    write_size_prefix(out, section_start, 0)?;
    Ok(())
}

fn write_merged_channel(
    out: &mut Output,
    rows: usize,
    stride: usize,
    counts: &mut [u8],
    src: &[u8],
    dst: &mut [u8],
) -> Result<()> {
    for i in 0..rows {
        let src_start = i * stride;
        let src_end = src_start + stride;
        let length = rle_compress(&src[src_start..src_end], dst)?;
        out.write_bytes(&dst[0..length])?;

        let count_bytes = u16::try_from(length)?.to_be_bytes();
        counts[i * 2] = count_bytes[0];
        counts[i * 2 + 1] = count_bytes[1];
    }
    Ok(())
}

fn write_merged_image(cs: &CanvasState, dc: *mut DP_DrawContext, mut out: Output) -> Result<()> {
    out.write_bytes(&[0, 1])?; // Compression type: run-length encoding.

    let rows = usize::try_from(cs.height())?;
    let stride = usize::try_from(cs.width())?;
    let counts_stride = rows * 2;
    let counts_length = counts_stride * 4;
    let pool_size = counts_length + stride * 2;
    let pool: &mut [u8] = unsafe {
        from_raw_parts_mut(
            DP_draw_context_pool_require(dc, pool_size).cast(),
            pool_size,
        )
    };

    let (counts, dst) = pool.split_at_mut(counts_length);
    let counts_pos = out.tell()?;
    clear_buffer(counts);
    out.write_bytes(counts)?;

    let size = rows * stride;
    let buffer = cs.to_flat_separated_urgba8()?;
    for i in 0..4 {
        let counts_start = i * counts_stride;
        let counts_end = counts_start + counts_stride;
        let buffer_start = i * size;
        let buffer_end = buffer_start + size;
        write_merged_channel(
            &mut out,
            rows,
            stride,
            &mut counts[counts_start..counts_end],
            &buffer[buffer_start..buffer_end],
            dst,
        )?;
    }

    out.seek(counts_pos)?;
    out.write_bytes(counts)?;

    out.close()?;
    Ok(())
}

fn write_psd(cs: &CanvasState, dc: *mut DP_DrawContext, mut out: Output) -> Result<()> {
    // Magic "8BPS", 2 bytes for the version (0, 1), 6 reserved zero bytes.
    out.write_bytes(&[
        56, 66, 80, 83, // "8BPS" magic number.
        0, 1, // Version, always 1.
        0, 0, 0, 0, 0, 0, // Reserved, always 0.
        0, 4, // Number of channels, we always use RGBA, so it's 4.
    ])?;
    // Dimensions, height first for some reason. PSD supports at most 30,000
    // pixels in either direction, which is more than the u16 range we allow.
    out.write_u32_be(u32::try_from(cs.height())?)?;
    out.write_u32_be(u32::try_from(cs.width())?)?;
    out.write_bytes(&[
        0, 8, // Channel depth, we use 8 bit channels.
        0, 3, // Color mode, we always use 3 for RGB.
        0, 0, 0, 0, // Color mode section length, always zero for RGB.
        0, 0, 0, 0, // Image resources section length, we don't have any.
    ])?;
    write_layer_sections(cs, dc, &mut out)?;
    write_merged_image(cs, dc, out)?;
    Ok(())
}

fn save_psd(
    cs: *mut DP_CanvasState,
    path: *const c_char,
    dc: *mut DP_DrawContext,
) -> DP_SaveResult {
    if let Some(acs) = unsafe { cs.as_mut() }.map(CanvasState::new_attached) {
        let out = match Output::new_from_path(path) {
            Ok(o) => o,
            Err(_) => return DP_SAVE_RESULT_OPEN_ERROR,
        };
        match write_psd(&acs, dc, out) {
            Ok(()) => DP_SAVE_RESULT_SUCCESS,
            Err(_) => DP_SAVE_RESULT_WRITE_ERROR,
        }
    } else {
        DP_SAVE_RESULT_BAD_ARGUMENTS
    }
}

#[no_mangle]
pub extern "C" fn DP_save_psd(
    cs: *mut DP_CanvasState,
    path: *const c_char,
    dc: *mut DP_DrawContext,
) -> DP_SaveResult {
    if let Ok(save_result) = catch_unwind(|| save_psd(cs, path, dc)) {
        save_result
    } else {
        dp_error_set("Panic while saving PSD");
        DP_SAVE_RESULT_INTERNAL_ERROR
    }
}
