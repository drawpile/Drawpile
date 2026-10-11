// SPDX-License-Identifier: GPL-3.0-or-later
//! C bindings for building a raw RGBA image out of pixel data and compressing
//! it into an optimized PNG.

use oxipng::{BitDepth, ColorType, Deflater, Options, PngError, RawImage};
use std::{
    panic::{self, AssertUnwindSafe},
    ptr, slice,
};

#[repr(C)]
pub enum OxipngResult {
    Success = 0,
    NullPointer,
    InvalidBitDepth,
    InvalidDataLength,
    OptimizationFailed,
    Panic,
}

/// An image to be optimized, free it with `oxipng_raw_image_free`.
pub struct OxipngRawImage {
    image: RawImage,
}

/// A block of bytes owned by Rust, free it with `oxipng_buffer_free`.
pub struct OxipngBuffer {
    bytes: Vec<u8>,
}

/// Construct a new raw image definition with truecolor and alpha.
///
/// * `bit_depth` is the number of bits per channel: 8 or 16.
/// * `data` is `data_len` bytes of unfiltered, uninterlaced pixel data.
/// * `out_image` receives the image on success, free it with
///   `oxipng_raw_image_free`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn oxipng_raw_image_new(
    width: u32,
    height: u32,
    bit_depth: u8,
    data: *const u8,
    data_len: usize,
    out_image: *mut *mut OxipngRawImage,
) -> OxipngResult {
    if out_image.is_null() {
        return OxipngResult::NullPointer;
    }

    let bit_depth = match bit_depth {
        8 => BitDepth::Eight,
        16 => BitDepth::Sixteen,
        _ => return OxipngResult::InvalidBitDepth,
    };

    let data = if data_len == 0 {
        Vec::new()
    } else if data.is_null() {
        return OxipngResult::NullPointer;
    } else {
        unsafe { slice::from_raw_parts(data, data_len) }.to_vec()
    };

    match RawImage::new(width, height, ColorType::RGBA, bit_depth, data) {
        Ok(image) => {
            unsafe { *out_image = Box::into_raw(Box::new(OxipngRawImage { image })) };
            OxipngResult::Success
        }
        Err(err) => result_from_error(err),
    }
}

/// Free an image returned by `oxipng_raw_image_new`, NULL is allowed.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn oxipng_raw_image_free(image: *mut OxipngRawImage) {
    if !image.is_null() {
        drop(unsafe { Box::from_raw(image) });
    }
}

/// Create an optimized png from the given raw image.
///
/// * `level` is the optimization preset to use, from 0 (fastest) to 6
///   (smallest), values beyond 6 behave like 6.
/// * `out_buffer` receives the png file contents on success, free it with
///   `oxipng_buffer_free`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn oxipng_raw_image_create_optimized_png(
    image: *const OxipngRawImage,
    level: u8,
    deflate_level: u8,
    out_buffer: *mut *mut OxipngBuffer,
) -> OxipngResult {
    if out_buffer.is_null() {
        return OxipngResult::NullPointer;
    }

    let Some(image) = (unsafe { image.as_ref() }) else {
        return OxipngResult::NullPointer;
    };

    let mut opts = Options::from_preset(level);
    opts.optimize_alpha = true;
    opts.deflater = Deflater::Libdeflater {
        compression: deflate_level,
    };
    // An unwind across the FFI boundary would take the whole process down,
    // turn it into an error instead.
    let result = panic::catch_unwind(AssertUnwindSafe(|| image.image.create_optimized_png(&opts)));
    match result {
        Ok(Ok(bytes)) => {
            unsafe { *out_buffer = Box::into_raw(Box::new(OxipngBuffer { bytes })) };
            OxipngResult::Success
        }
        Ok(Err(err)) => result_from_error(err),
        Err(_) => OxipngResult::Panic,
    }
}

/// Get the bytes in the given buffer, NULL if the buffer is NULL.
///
/// The bytes belong to the buffer and are only valid until it is freed.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn oxipng_buffer_data(buffer: *const OxipngBuffer) -> *const u8 {
    match unsafe { buffer.as_ref() } {
        Some(buffer) => buffer.bytes.as_ptr(),
        None => ptr::null(),
    }
}

/// Get the number of bytes in the given buffer, 0 if the buffer is NULL.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn oxipng_buffer_size(buffer: *const OxipngBuffer) -> usize {
    match unsafe { buffer.as_ref() } {
        Some(buffer) => buffer.bytes.len(),
        None => 0,
    }
}

/// Free a buffer returned by `oxipng_raw_image_create_optimized_png`, NULL is
/// allowed.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn oxipng_buffer_free(buffer: *mut OxipngBuffer) {
    if !buffer.is_null() {
        drop(unsafe { Box::from_raw(buffer) });
    }
}

fn result_from_error(err: PngError) -> OxipngResult {
    match err {
        PngError::InvalidDepthForType(_, _) => OxipngResult::InvalidBitDepth,
        PngError::IncorrectDataLength(_, _) => OxipngResult::InvalidDataLength,
        _ => OxipngResult::OptimizationFailed,
    }
}
