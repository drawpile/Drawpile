#pragma once

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

typedef enum OxipngResult {
  OxipngResult_Success = 0,
  OxipngResult_NullPointer,
  OxipngResult_InvalidBitDepth,
  OxipngResult_InvalidDataLength,
  OxipngResult_OptimizationFailed,
  OxipngResult_Panic,
} OxipngResult;

/**
 * A block of bytes owned by Rust, free it with `oxipng_buffer_free`.
 */
typedef struct OxipngBuffer OxipngBuffer;

/**
 * An image to be optimized, free it with `oxipng_raw_image_free`.
 */
typedef struct OxipngRawImage OxipngRawImage;

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

/**
 * Construct a new raw image definition with truecolor and alpha.
 *
 * * `bit_depth` is the number of bits per channel: 8 or 16.
 * * `data` is `data_len` bytes of unfiltered, uninterlaced pixel data.
 * * `out_image` receives the image on success, free it with
 *   `oxipng_raw_image_free`.
 */
enum OxipngResult oxipng_raw_image_new(uint32_t width,
                                       uint32_t height,
                                       uint8_t bit_depth,
                                       const uint8_t *data,
                                       uintptr_t data_len,
                                       struct OxipngRawImage **out_image);

/**
 * Free an image returned by `oxipng_raw_image_new`, NULL is allowed.
 */
void oxipng_raw_image_free(struct OxipngRawImage *image);

/**
 * Create an optimized png from the given raw image.
 *
 * * `level` is the optimization preset to use, from 0 (fastest) to 6
 *   (smallest), values beyond 6 behave like 6.
 * * `out_buffer` receives the png file contents on success, free it with
 *   `oxipng_buffer_free`.
 */
enum OxipngResult oxipng_raw_image_create_optimized_png(const struct OxipngRawImage *image,
                                                        uint8_t level,
                                                        struct OxipngBuffer **out_buffer);

/**
 * Get the bytes in the given buffer, NULL if the buffer is NULL.
 *
 * The bytes belong to the buffer and are only valid until it is freed.
 */
const uint8_t *oxipng_buffer_data(const struct OxipngBuffer *buffer);

/**
 * Get the number of bytes in the given buffer, 0 if the buffer is NULL.
 */
uintptr_t oxipng_buffer_size(const struct OxipngBuffer *buffer);

/**
 * Free a buffer returned by `oxipng_raw_image_create_optimized_png`, NULL is
 * allowed.
 */
void oxipng_buffer_free(struct OxipngBuffer *buffer);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus
