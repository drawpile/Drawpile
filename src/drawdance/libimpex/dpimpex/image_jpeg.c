// SPDX-License-Identifier: GPL-3.0-or-later
#include "image_jpeg.h"
#include "image.h"
#include "pixels.h"
#include <dpcommon/common.h>
#include <dpcommon/conversions.h>
#include <dpcommon/input.h>
#include <dpcommon/output.h>
#include <setjmp.h>
#include <stdio.h>
// These must be included last.
#include <jerror.h>
#include <jpeglib.h>

static_assert(sizeof(JOCTET) == 1, "JOCTET is char-sized");
static_assert(sizeof(JSAMPLE) == 1, "JSAMPLE is char-sized");

#define BUFFER_SIZE 4098


typedef struct DP_JpegErrorMgr {
    struct jpeg_error_mgr parent;
    jmp_buf env;
} DP_JpegErrorMgr;

static void handle_jpeg_error(j_common_ptr cinfo)
{
    char buffer[JMSG_LENGTH_MAX];
    cinfo->err->format_message(cinfo, buffer);
    DP_error_set("%s", buffer);
    DP_JpegErrorMgr *jerr = (DP_JpegErrorMgr *)cinfo->err;
    longjmp(jerr->env, 1);
}


typedef struct DP_JpegSourceMgr {
    struct jpeg_source_mgr parent;
    JOCTET *buffer;
    DP_Input *input;
} DP_JpegSourceMgr;

static void init_jpeg_source_mgr(j_decompress_ptr cinfo)
{
    DP_JpegSourceMgr *src = (DP_JpegSourceMgr *)cinfo->src;
    src->buffer = DP_malloc(BUFFER_SIZE);
    src->parent.next_input_byte = NULL;
    src->parent.bytes_in_buffer = 0;
}

static boolean fill_jpeg_source_input_buffer(j_decompress_ptr cinfo)
{
    DP_JpegSourceMgr *src = (DP_JpegSourceMgr *)cinfo->src;
    bool error;
    size_t read = DP_input_read(src->input, src->buffer, BUFFER_SIZE, &error);
    if (error) {
        ERREXIT(cinfo, JERR_FILE_READ);
        DP_UNREACHABLE();
    }
    // Insert an end of input marker if we didn't read any further data. This
    // is what libjpeg's standard decoders do too. The alternative would be to
    // raise an error instead, otherwise libjpeg would keep asking for new data.
    if (read == 0) {
        WARNMS(cinfo, JWRN_JPEG_EOF);
        src->buffer[0] = 0xff;
        src->buffer[1] = JPEG_EOI;
        src->parent.bytes_in_buffer = 2;
    }
    else {
        src->parent.bytes_in_buffer = read;
    }
    src->parent.next_input_byte = src->buffer;
    return TRUE;
}

static void skip_jpeg_source_input_data(j_decompress_ptr cinfo, long num_bytes)
{
    // Based on libjeg's standard decoders, they skip like this too.
    DP_JpegSourceMgr *src = (DP_JpegSourceMgr *)cinfo->src;
    if (num_bytes > 0) {
        while (num_bytes > (long)src->parent.bytes_in_buffer) {
            num_bytes -= (long)src->parent.bytes_in_buffer;
            (void)(*src->parent.fill_input_buffer)(cinfo);
        }
        src->parent.next_input_byte += (size_t)num_bytes;
        src->parent.bytes_in_buffer -= (size_t)num_bytes;
    }
}

static void term_jpeg_source(DP_UNUSED j_decompress_ptr cinfo)
{
    // Nothing to do.
}

DP_Image *DP_image_jpeg_read(DP_Input *input)
{
    DP_ASSERT(input);
    struct jpeg_decompress_struct cinfo;

    DP_JpegErrorMgr jerr;
    cinfo.err = jpeg_std_error(&jerr.parent);
    jerr.parent.error_exit = handle_jpeg_error;

    DP_JpegSourceMgr src = {
        {NULL, 0, init_jpeg_source_mgr, fill_jpeg_source_input_buffer,
         skip_jpeg_source_input_data, jpeg_resync_to_restart, term_jpeg_source},
        NULL,
        input};

    DP_Image *img = NULL;

    if (setjmp(jerr.env)) {
        jpeg_destroy_decompress(&cinfo);
        DP_image_free(img);
        DP_free(src.buffer);
        return false;
    }

    jpeg_create_decompress(&cinfo);
    cinfo.src = &src.parent;
    jpeg_read_header(&cinfo, TRUE);
    cinfo.out_color_space = JCS_RGB;
    jpeg_start_decompress(&cinfo);

    if (cinfo.output_components != 3) {
        DP_error_set("Invalid color components: %d",
                     (int)cinfo.output_components);
        longjmp(jerr.env, 1);
    }

    bool dimensions_ok =
        cinfo.output_width > 0 && cinfo.output_width <= INT16_MAX
        && cinfo.output_height > 0 && cinfo.output_width <= INT16_MAX;
    if (dimensions_ok) {
        img = DP_image_new((int)cinfo.output_width, (int)cinfo.output_height);
    }
    else {
        DP_error_set("Invalid image dimensions: %ux%u",
                     (unsigned int)cinfo.output_width,
                     (unsigned int)cinfo.output_height);
        longjmp(jerr.env, 1);
    }

    JSAMPARRAY buffer = (*cinfo.mem->alloc_sarray)(
        (j_common_ptr)&cinfo, JPOOL_IMAGE, cinfo.output_width * 3, 1);

    DP_Pixel8 *pixels = DP_image_pixels(img);
    while (cinfo.output_scanline < cinfo.output_height) {
        jpeg_read_scanlines(&cinfo, buffer, 1);
        for (JDIMENSION i = 0; i < cinfo.output_width; ++i) {
            *pixels = (DP_Pixel8){
                .b = buffer[0][i * 3 + 2],
                .g = buffer[0][i * 3 + 1],
                .r = buffer[0][i * 3 + 0],
                .a = 0xff,
            };
            ++pixels;
        }
    }

    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
    DP_free(src.buffer);
    return img;
}


typedef struct DP_JpegDestinationMgr {
    struct jpeg_destination_mgr parent;
    JOCTET *buffer;
    DP_Output *output;
} DP_JpegDestinationMgr;

static void init_jpeg_destination_mgr(j_compress_ptr cinfo)
{
    DP_JpegDestinationMgr *dest = (DP_JpegDestinationMgr *)cinfo->dest;
    dest->buffer = DP_malloc(BUFFER_SIZE);
    dest->parent.next_output_byte = dest->buffer;
    dest->parent.free_in_buffer = BUFFER_SIZE;
}

static boolean empty_jpeg_destination_buffer(j_compress_ptr cinfo)
{
    DP_JpegDestinationMgr *dest = (DP_JpegDestinationMgr *)cinfo->dest;
    if (DP_output_write(dest->output, dest->buffer, BUFFER_SIZE)) {
        dest->parent.next_output_byte = dest->buffer;
        dest->parent.free_in_buffer = BUFFER_SIZE;
        return TRUE;
    }
    else {
        ERREXIT(cinfo, JERR_FILE_WRITE);
        DP_UNREACHABLE();
    }
}

static void term_jpeg_destination_buffer(j_compress_ptr cinfo)
{
    DP_JpegDestinationMgr *dest = (DP_JpegDestinationMgr *)cinfo->dest;
    size_t size = BUFFER_SIZE - dest->parent.free_in_buffer;
    bool ok = (size == 0 || DP_output_write(dest->output, dest->buffer, size))
           && DP_output_flush(dest->output);
    if (!ok) {
        ERREXIT(cinfo, JERR_FILE_WRITE);
        DP_UNREACHABLE();
    }
}

bool DP_image_jpeg_write(DP_Output *output, int width, int height,
                         DP_Pixel8 *pixels, int quality)
{
    DP_ASSERT(output);
    DP_ASSERT(width > 0);
    DP_ASSERT(height > 0);
    DP_ASSERT(pixels);

    struct jpeg_compress_struct cinfo;

    DP_JpegErrorMgr jerr;
    cinfo.err = jpeg_std_error(&jerr.parent);
    jerr.parent.error_exit = handle_jpeg_error;

    DP_JpegDestinationMgr dest = {{NULL, 0, init_jpeg_destination_mgr,
                                   empty_jpeg_destination_buffer,
                                   term_jpeg_destination_buffer},
                                  NULL,
                                  output};

    JSAMPROW scanline = DP_malloc(DP_int_to_size(width) * 3u);

    if (setjmp(jerr.env)) {
        jpeg_destroy_compress(&cinfo);
        DP_free(scanline);
        DP_free(dest.buffer);
        return false;
    }

    jpeg_create_compress(&cinfo);
    cinfo.dest = &dest.parent;
    cinfo.image_width = (JDIMENSION)width;
    cinfo.image_height = (JDIMENSION)height;
    cinfo.input_components = 3;
    cinfo.in_color_space = JCS_RGB;
    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, quality, TRUE);
    jpeg_start_compress(&cinfo, TRUE);

    while (cinfo.next_scanline < cinfo.image_height) {
        JDIMENSION offset = cinfo.next_scanline * cinfo.image_width;
        for (JDIMENSION i = 0; i < cinfo.image_width; ++i) {
            DP_UPixel8 pixel = DP_pixel8_unpremultiply(pixels[offset + i]);
            scanline[i * 3 + 0] = pixel.r;
            scanline[i * 3 + 1] = pixel.g;
            scanline[i * 3 + 2] = pixel.b;
        }
        jpeg_write_scanlines(&cinfo, &scanline, 1);
    }

    jpeg_finish_compress(&cinfo);

    jpeg_destroy_compress(&cinfo);
    DP_free(scanline);
    DP_free(dest.buffer);
    return true;
}
