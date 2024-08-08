// SPDX-License-Identifier: GPL-3.0-or-later
#include "image_webp.h"
#include <dpcommon/common.h>
#include <dpcommon/conversions.h>
#include <dpcommon/input.h>
#include <dpcommon/output.h>
#include <dpengine/image.h>
#include <dpengine/pixels.h>
#include <webp/decode.h>
#include <webp/encode.h>


static const char *status_code_to_string(VP8StatusCode status)
{
    switch (status) {
    case VP8_STATUS_OK:
        return "OK";
    case VP8_STATUS_OUT_OF_MEMORY:
        return "OUT_OF_MEMORY";
    case VP8_STATUS_INVALID_PARAM:
        return "INVALID_PARAM";
    case VP8_STATUS_BITSTREAM_ERROR:
        return "BITSTREAM_ERROR";
    case VP8_STATUS_UNSUPPORTED_FEATURE:
        return "UNSUPPORTED_FEATURE";
    case VP8_STATUS_SUSPENDED:
        return "SUSPENDED";
    case VP8_STATUS_USER_ABORT:
        return "USER_ABORT";
    case VP8_STATUS_NOT_ENOUGH_DATA:
        return "NOT_ENOUGH_DATA";
    }
    return "UNKNOWN";
}

DP_Image *DP_image_webp_read(DP_Input *input)
{
    WebPDecBuffer wdb;
    if (!WebPInitDecBuffer(&wdb)) {
        DP_error_set("Error initializing decode buffer");
        return NULL;
    }
    wdb.colorspace = MODE_bgrA;

    WebPIDecoder *idec = WebPINewDecoder(&wdb);
    if (!idec) {
        WebPFreeDecBuffer(&wdb);
        DP_error_set("Error creating decoder");
        return NULL;
    }

    unsigned char buf[BUFSIZ];
    while (true) {
        bool error = false;
        size_t read = DP_input_read(input, buf, sizeof(buf), &error);
        if (error) {
            WebPIDelete(idec);
            WebPFreeDecBuffer(&wdb);
            return NULL;
        }
        else if (read == 0) {
            DP_error_set("Premature end of file");
            WebPIDelete(idec);
            WebPFreeDecBuffer(&wdb);
            return NULL;
        }
        else {
            VP8StatusCode status = WebPIAppend(idec, buf, read);
            if (status == VP8_STATUS_OK) {
                break;
            }
            else if (status != VP8_STATUS_SUSPENDED) {
                DP_error_set("Decode error %d (%s)", (int)status,
                             status_code_to_string(status));
                WebPIDelete(idec);
                WebPFreeDecBuffer(&wdb);
                return NULL;
            }
        }
    }

    WebPIDelete(idec);

    DP_Image *img;
    int w = wdb.width;
    int h = wdb.height;
    if (w > 0 && h > 0) {
        img = DP_image_new(w, h);
        DP_Pixel8 *pixels = DP_image_pixels(img);
        const uint8_t *rgba = wdb.u.RGBA.rgba;
        size_t rgba_stride = DP_int_to_size(wdb.u.RGBA.stride);
        size_t img_stride = DP_int_to_size(w) * (size_t)4;
        if (rgba_stride == img_stride) {
            memcpy(pixels, rgba, img_stride * DP_int_to_size(h));
        }
        else {
            for (int i = 0; i < h; ++i) {
                memcpy(pixels + w * i, rgba + rgba_stride * DP_int_to_size(i),
                       img_stride);
            }
        }
    }
    else {
        img = NULL;
        DP_error_set("Image has a size of zero");
    }

    WebPFreeDecBuffer(&wdb);
    return img;
}


static int write_webp(const uint8_t *data, size_t data_size,
                      const WebPPicture *picture)
{
    DP_Output *output = picture->custom_ptr;
    return DP_output_write(output, data, data_size);
}

static const char *encode_error_to_string(WebPEncodingError error)
{
    switch (error) {
    case VP8_ENC_OK:
        return "OK";
    case VP8_ENC_ERROR_OUT_OF_MEMORY:
        return "ERROR_OUT_OF_MEMORY";
    case VP8_ENC_ERROR_BITSTREAM_OUT_OF_MEMORY:
        return "ERROR_BITSTREAM_OUT_OF_MEMORY";
    case VP8_ENC_ERROR_NULL_PARAMETER:
        return "ERROR_NULL_PARAMETER";
    case VP8_ENC_ERROR_INVALID_CONFIGURATION:
        return "ERROR_INVALID_CONFIGURATION";
    case VP8_ENC_ERROR_BAD_DIMENSION:
        return "ERROR_BAD_DIMENSION";
    case VP8_ENC_ERROR_PARTITION0_OVERFLOW:
        return "ERROR_PARTITION0_OVERFLOW";
    case VP8_ENC_ERROR_PARTITION_OVERFLOW:
        return "ERROR_PARTITION_OVERFLOW";
    case VP8_ENC_ERROR_BAD_WRITE:
        return "ERROR_BAD_WRITE";
    case VP8_ENC_ERROR_FILE_TOO_BIG:
        return "ERROR_FILE_TOO_BIG";
    case VP8_ENC_ERROR_USER_ABORT:
        return "ERROR_USER_ABORT";
    case VP8_ENC_ERROR_LAST:
        return "ERROR_LAST";
    }
    return "UNKNOWN";
}

bool DP_image_webp_write(DP_Output *output, int width, int height,
                         DP_Pixel8 *pixels)
{
    WebPConfig config;
    if (!WebPConfigPreset(&config, WEBP_PRESET_DRAWING, 100.0f)) {
        DP_error_set("Error initializing encoding config");
        return false;
    }

    config.lossless = true;
    if (!WebPValidateConfig(&config)) {
        DP_error_set("Error validating encoding config");
        return false;
    }

    WebPPicture pic;
    if (!WebPPictureInit(&pic)) {
        DP_error_set("Error initializing picture");
        return false;
    }

    int pixels_length = width * height;
    DP_UPixel8 *upixels = DP_malloc(DP_int_to_size(pixels_length) * 4);
    for (int i = 0; i < pixels_length; ++i) {
        upixels[i] = DP_pixel8_unpremultiply(pixels[i]);
    }

    pic.width = width;
    pic.height = height;
    pic.use_argb = true;
    pic.argb = (uint32_t *)upixels;
    pic.argb_stride = width;
    pic.writer = write_webp;
    pic.custom_ptr = output;

    int ok = WebPEncode(&config, &pic);
    DP_free(upixels);
    if (!ok) {
        if (pic.error_code != VP8_ENC_ERROR_BAD_WRITE) {
            DP_error_set("Encode error %d (%s)", (int)pic.error_code,
                         encode_error_to_string(pic.error_code));
        }
        return false;
    }

    return DP_output_flush(output);
}
