// SPDX-License-Identifier: GPL-3.0-or-later
#include "save_video.h"
#include "process.h"
#include "save.h"
#include <dpcommon/common.h>
#include <dpcommon/conversions.h>
#include <dpcommon/geom.h>
#include <dpcommon/output.h>
#include <dpcommon/vector.h>
#include <dpengine/canvas_state.h>
#include <dpengine/image.h>
#include <dpengine/view_mode.h>
#include <libavcodec/avcodec.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
#ifdef DP_ANDROID_VIDEO_ENCODER
#    include "android_video_encoder.h"
#endif
#ifdef DP_WINDOWS_VIDEO_ENCODER
#    include "windows_video_encoder.h"
#endif
#if defined(DP_ANDROID_VIDEO_ENCODER) || defined(DP_WINDOWS_VIDEO_ENCODER)
#    include <libavutil/imgutils.h>
#endif


// Same as IO_BUFFER_SIZE in ffmpeg's avio.c.
#define BUFFER_SIZE 32768

// FFMPEG broke their API in a weird way when they added const and then switched
// up how you check for it. We just cast away constness from pointers when
// passing them to functions to get around that.
#define REMOVE_CONST(PTR) ((void *)(PTR))

// More FFMPEG API breakage. They added constness to a callback and changed
// their mind on which macro you use to check for it or something.
#if defined(ff_const59)
typedef ff_const59 uint8_t *DP_WriteAvioPacketBuf;
#elif FF_API_AVIO_WRITE_NONCONST
typedef uint8_t *DP_WriteAvioPacketBuf;
#else
typedef const uint8_t *DP_WriteAvioPacketBuf;
#endif


static bool is_destination_ok(DP_SaveVideoDestination destination,
                              void *destination_param)
{
    switch (destination) {
    case DP_SAVE_VIDEO_DESTINATION_PATH:
    case DP_SAVE_VIDEO_DESTINATION_WINDOWS: {
        const char *path = destination_param;
        return path && path[0] != '\0';
    }
    case DP_SAVE_VIDEO_DESTINATION_OUTPUT: {
        DP_Output *output = destination_param;
        return output != NULL;
    }
    case DP_SAVE_VIDEO_DESTINATION_FFMPEG: {
        const DP_SaveVideoFfmpegParams *ffmpeg = destination_param;
        return ffmpeg && ffmpeg->program && ffmpeg->program[0] != '\0'
            && ffmpeg->output && ffmpeg->output[0] != '\0';
    }
    case DP_SAVE_VIDEO_DESTINATION_ANDROID: {
        const DP_SaveVideoAndroidParams *android = destination_param;
        return android && android->output && android->output[0] != '\0'
            && android->temp && android->temp[0] != '\0';
    }
    default:
        return false;
    }
}

static enum AVCodecID get_format_codec_id(int format)
{
    switch (format) {
    case DP_SAVE_VIDEO_FORMAT_MP4_VP9:
        return AV_CODEC_ID_VP9;
    case DP_SAVE_VIDEO_FORMAT_WEBM_VP8:
        return AV_CODEC_ID_VP8;
    case DP_SAVE_VIDEO_FORMAT_WEBP:
        return AV_CODEC_ID_WEBP;
    case DP_SAVE_VIDEO_FORMAT_PALETTE:
        return AV_CODEC_ID_RAWVIDEO;
    case DP_SAVE_VIDEO_FORMAT_GIF:
        return AV_CODEC_ID_GIF;
    case DP_SAVE_VIDEO_FORMAT_MP4_H264:
        return AV_CODEC_ID_H264;
    case DP_SAVE_VIDEO_FORMAT_MP4_AV1:
        return AV_CODEC_ID_AV1;
    case DP_SAVE_VIDEO_FORMAT_APNG:
        return AV_CODEC_ID_APNG;
    default:
        return AV_CODEC_ID_NONE;
    }
}


static DP_Rect get_crop(DP_CanvasState *cs, const DP_Rect *area)
{
    int width = DP_canvas_state_width(cs);
    int height = DP_canvas_state_height(cs);
    DP_Rect canvas_area = DP_rect_make(0, 0, width, height);
    return area ? DP_rect_intersection(canvas_area, *area) : canvas_area;
}

static const char *get_format_name(int format)
{
    switch (format) {
    case DP_SAVE_VIDEO_FORMAT_MP4_VP9:
    case DP_SAVE_VIDEO_FORMAT_MP4_H264:
    case DP_SAVE_VIDEO_FORMAT_MP4_AV1:
        return "mp4";
    case DP_SAVE_VIDEO_FORMAT_WEBM_VP8:
        return "webm";
    case DP_SAVE_VIDEO_FORMAT_WEBP:
        return "webp";
    case DP_SAVE_VIDEO_FORMAT_PALETTE:
        return "rawvideo";
    case DP_SAVE_VIDEO_FORMAT_GIF:
        return "gif";
    case DP_SAVE_VIDEO_FORMAT_APNG:
        return "apng";
    default:
        return NULL;
    }
}

static const AVCodec *get_codec_encoder(enum AVCodecID codec_id)
{
    // We get libaom by default, but libsvtav1 is way faster.
    if (codec_id == AV_CODEC_ID_AV1) {
        const AVCodec *codec = avcodec_find_encoder_by_name("libsvtav1");
        if (codec) {
            return codec;
        }
    }
    return avcodec_find_encoder(codec_id);
}

static bool check_format_dimensions(int output_width, int output_height,
                                    int format)
{
    switch (format) {
    case DP_SAVE_VIDEO_FORMAT_MP4_VP9:
    case DP_SAVE_VIDEO_FORMAT_WEBM_VP8:
    case DP_SAVE_VIDEO_FORMAT_GIF:
    case DP_SAVE_VIDEO_FORMAT_MP4_H264:
    case DP_SAVE_VIDEO_FORMAT_MP4_AV1:
        return DP_save_check_width_height(output_width, output_height, 65536);
    case DP_SAVE_VIDEO_FORMAT_WEBP:
        return DP_save_check_width_height(output_width, output_height, 16384);
    case DP_SAVE_VIDEO_FORMAT_APNG:
    default:
        return true;
    }
}

static int get_format_loops(int format, int loops)
{
    switch (format) {
    case DP_SAVE_VIDEO_FORMAT_WEBP:
    case DP_SAVE_VIDEO_FORMAT_PALETTE:
    case DP_SAVE_VIDEO_FORMAT_GIF:
    case DP_SAVE_VIDEO_FORMAT_APNG:
        return 1;
    default:
        return loops < 1 ? 1 : loops;
    }
}

static int get_format_dimension(int format, int dimension)
{
    switch (format) {
    case DP_SAVE_VIDEO_FORMAT_MP4_H264:
        // H264 dimensions must be divisible by 2.
        return dimension + dimension % 2;
    default:
        return dimension;
    }
}

static int get_format_codec_dimension(int format, int dimension)
{
    switch (format) {
    case DP_SAVE_VIDEO_FORMAT_PALETTE:
        return DP_SAVE_VIDEO_GIF_PALETTE_DIMENSION;
    default:
        return dimension;
    }
}

static int get_scaling_flags(unsigned int flags, int input_width,
                             int input_height, int output_width,
                             int output_height)
{
    // Only scale smoothly if it was requested and the difference is notable. A
    // 1 pixel difference may be due to get_format_dimensions padding.
    if ((flags & DP_SAVE_VIDEO_FLAGS_SCALE_SMOOTH)
        && abs(input_width - output_width) > 1
        && abs(input_height - output_height) > 1) {
        return SWS_BILINEAR;
    }
    else {
        return 0;
    }
}

static int get_format_pix_fmt(int format, bool frame)
{
    switch (format) {
    case DP_SAVE_VIDEO_FORMAT_WEBP:
        return AV_PIX_FMT_BGRA;
    case DP_SAVE_VIDEO_FORMAT_PALETTE:
        return AV_PIX_FMT_BGRA;
    case DP_SAVE_VIDEO_FORMAT_GIF:
        return frame ? AV_PIX_FMT_BGRA : AV_PIX_FMT_PAL8;
    case DP_SAVE_VIDEO_FORMAT_APNG:
        return AV_PIX_FMT_RGBA;
    default:
        return AV_PIX_FMT_YUV420P;
    }
}

static void set_option(void *obj, const char *name, const char *value)
{
    int result = av_opt_set(obj, name, value, AV_OPT_SEARCH_CHILDREN);
    if (result == 0) {
        DP_debug("Set option %s to %s", name, value);
    }
    else {
        DP_warn("Error setting option %s to %s: %s", name, value,
                av_err2str(result));
    }
}

static void set_format_codec_params(int format, AVCodecContext *codec_context)
{
    switch (format) {
    case DP_SAVE_VIDEO_FORMAT_PALETTE:
    case DP_SAVE_VIDEO_FORMAT_GIF:
    case DP_SAVE_VIDEO_FORMAT_APNG:
        break;
    case DP_SAVE_VIDEO_FORMAT_WEBM_VP8:
        codec_context->bit_rate = 50 * 1024 * 1024;
        set_option(codec_context->priv_data, "crf", "15");
        break;
    case DP_SAVE_VIDEO_FORMAT_MP4_VP9:
        codec_context->bit_rate = 0;
        set_option(codec_context->priv_data, "crf", "20");
        break;
    case DP_SAVE_VIDEO_FORMAT_WEBP:
        set_option(codec_context->priv_data, "lossless", "1");
        set_option(codec_context->priv_data, "preset", "drawing");
        set_option(codec_context->priv_data, "quality", "100");
        break;
    case DP_SAVE_VIDEO_FORMAT_MP4_H264:
        set_option(codec_context->priv_data, "crf", "19");
        set_option(codec_context->priv_data, "tune", "animation");
        break;
    case DP_SAVE_VIDEO_FORMAT_MP4_AV1:
        codec_context->bit_rate = 0;
        set_option(codec_context->priv_data, "crf", "23");
        break;
    default:
        DP_warn("Don't know format params for format %d", format);
        break;
    }
}

static void set_format_output_params(int format,
                                     AVFormatContext *format_context)
{
    switch (format) {
    case DP_SAVE_VIDEO_FORMAT_MP4_VP9:
    case DP_SAVE_VIDEO_FORMAT_WEBM_VP8:
    case DP_SAVE_VIDEO_FORMAT_PALETTE:
    case DP_SAVE_VIDEO_FORMAT_GIF:
    case DP_SAVE_VIDEO_FORMAT_MP4_H264:
    case DP_SAVE_VIDEO_FORMAT_MP4_AV1:
        break;
    case DP_SAVE_VIDEO_FORMAT_WEBP:
        set_option(format_context->priv_data, "loop", "0");
        break;
    case DP_SAVE_VIDEO_FORMAT_APNG:
        set_option(format_context->priv_data, "plays", "0");
        break;
    default:
        DP_warn("Don't know format params for format %d", format);
        break;
    }
}

static const char *get_format_filter(int format)
{
    switch (format) {
    case DP_SAVE_VIDEO_FORMAT_PALETTE:
        return "palettegen=stats_mode=full";
    case DP_SAVE_VIDEO_FORMAT_GIF:
        return "[in][palette]paletteuse[out]";
    default:
        return NULL;
    }
}

static const enum AVPixelFormat *get_format_filter_pix_fmts(int format)
{
    static const enum AVPixelFormat gif_formats[] = {AV_PIX_FMT_PAL8,
                                                     AV_PIX_FMT_NONE};
    static const enum AVPixelFormat default_formats[] = {AV_PIX_FMT_BGRA,
                                                         AV_PIX_FMT_NONE};
    switch (format) {
    case DP_SAVE_VIDEO_FORMAT_GIF:
        return gif_formats;
    default:
        return default_formats;
    }
}

static unsigned int get_format_flat_image_flags(int format)
{
    switch (format) {
    case DP_SAVE_VIDEO_FORMAT_PALETTE:
    case DP_SAVE_VIDEO_FORMAT_GIF:
        return DP_FLAT_IMAGE_RENDER_FLAGS | DP_FLAT_IMAGE_ONE_BIT_ALPHA;
    case DP_SAVE_VIDEO_FORMAT_WEBP:
    case DP_SAVE_VIDEO_FORMAT_APNG:
        return DP_FLAT_IMAGE_RENDER_FLAGS | DP_FLAT_IMAGE_UNPREMULTIPLY;
    default:
        return DP_FLAT_IMAGE_RENDER_FLAGS;
    }
}

static int write_avio_packet(void *opaque, DP_WriteAvioPacketBuf buf,
                             int buf_size)
{
    DP_Output *output = opaque;
    if (DP_output_write(output, buf, DP_int_to_size(buf_size))) {
        return buf_size;
    }
    else {
        return AVERROR(EIO);
    }
}

static int64_t seek_set_avio(DP_Output *output, int64_t offset)
{
    size_t effective_offset = offset < 0 ? 0 : DP_int64_to_size(offset);
    if (DP_output_seek(output, effective_offset)) {
        return DP_size_to_int64(effective_offset);
    }
    else {
        return AVERROR(EIO);
    }
}

static int64_t seek_cur_avio(DP_Output *output, int64_t offset)
{
    bool error;
    size_t pos = DP_output_tell(output, &error);
    if (error) {
        return AVERROR(EIO);
    }

    size_t effective_offset;
    if (offset >= 0) {
        effective_offset = pos + DP_int64_to_size(offset);
    }
    else {
        size_t negative_offset = DP_int64_to_size(-offset);
        if (negative_offset < pos) {
            effective_offset = pos - negative_offset;
        }
        else {
            effective_offset = 0;
        }
    }

    if (effective_offset != pos && !DP_output_seek(output, effective_offset)) {
        return AVERROR(EIO);
    }

    return DP_size_to_int64(effective_offset);
}

static int64_t seek_avio(void *opaque, int64_t offset, int whence)
{
    if (whence & AVSEEK_SIZE) {
        DP_warn("AVSEEK_SIZE unsupported with whence %d", whence);
        return 0;
    }
    else {
        switch (whence & ~AVSEEK_FORCE) {
        case SEEK_SET:
            return seek_set_avio(opaque, offset);
        case SEEK_CUR:
            return seek_cur_avio(opaque, offset);
        default:
            DP_warn("Unsupported seek whence %d", whence);
            return AVERROR(EINVAL);
        }
    }
}

static bool report_progress(DP_SaveProgressFn progress_fn, void *user,
                            double progress)
{
    return !progress_fn || progress_fn(user, progress);
}

static DP_SaveResult handle_frame(AVCodecContext *codec_context,
                                  AVFormatContext *format_context,
                                  AVStream *stream, AVFrame *frame,
                                  AVPacket *packet)
{
    int err = avcodec_send_frame(codec_context, frame);
    if (err != 0) {
        DP_error_set("Error sending frame: %s", av_err2str(err));
        return DP_SAVE_RESULT_INTERNAL_ERROR;
    }

    while (true) {
        err = avcodec_receive_packet(codec_context, packet);
        if (err == AVERROR(EAGAIN) || err == AVERROR_EOF) {
            return DP_SAVE_RESULT_SUCCESS;
        }
        else if (err != 0) {
            DP_error_set("Error receiving packet: %s", av_err2str(err));
            return DP_SAVE_RESULT_INTERNAL_ERROR;
        }

        av_packet_rescale_ts(packet, codec_context->time_base,
                             stream->time_base);
        packet->stream_index = stream->index;

        err = av_interleaved_write_frame(format_context, packet);
        if (err != 0) {
            DP_error_set("Error writing frame: %s", av_err2str(err));
            return DP_SAVE_RESULT_WRITE_ERROR;
        }
    }
}

static DP_SaveResult
filter_frame(AVCodecContext *codec_context, AVFormatContext *format_context,
             AVStream *stream, AVFrame *frame, AVPacket *packet,
             AVFilterContext *buffersrc_context,
             AVFilterContext *buffersink_context, AVFrame *filtered_frame)
{
    if (buffersrc_context) {
        DP_ASSERT(buffersink_context);
        DP_ASSERT(filtered_frame);
        int err = av_buffersrc_add_frame_flags(buffersrc_context, frame,
                                               AV_BUFFERSRC_FLAG_KEEP_REF);
        if (err < 0) {
            DP_error_set("Error feeding filter: %s", av_err2str(err));
            return DP_SAVE_RESULT_INTERNAL_ERROR;
        }

        while (true) {
            err = av_buffersink_get_frame(buffersink_context, filtered_frame);
            if (err == AVERROR(EAGAIN) || err == AVERROR_EOF) {
                return DP_SAVE_RESULT_SUCCESS;
            }
            else if (err != 0) {
                DP_error_set("Errorgetting filter frame: %s", av_err2str(err));
                return DP_SAVE_RESULT_INTERNAL_ERROR;
            }

            DP_SaveResult result = handle_frame(codec_context, format_context,
                                                stream, filtered_frame, packet);
            av_frame_unref(filtered_frame);
            if (result != DP_SAVE_RESULT_SUCCESS) {
                return result;
            }
        }
    }
    else {
        DP_ASSERT(!buffersink_context);
        DP_ASSERT(!filtered_frame);
        return handle_frame(codec_context, format_context, stream, frame,
                            packet);
    }
}

static DP_SaveResult save_video_libav(DP_SaveVideoParams params)
{
    DP_SaveResult result = DP_SAVE_RESULT_SUCCESS;
    AVCodecContext *codec_context = NULL;
    AVFormatContext *format_context = NULL;
    AVCodecParameters *codec_parameters = NULL;
    AVFilterInOut *filter_palette_outputs = NULL;
    AVFilterInOut *filter_outputs = NULL;
    AVFilterInOut *filter_inputs = NULL;
    AVFilterGraph *graph_context = NULL;
    AVFilterContext *buffersrc_context = NULL;
    AVFilterContext *palettesrc_context = NULL;
    AVFilterContext *buffersink_context = NULL;
    DP_Output *output = NULL;
    AVFrame *palette_frame = NULL;
    AVFrame *frame = NULL;
    AVFrame *filtered_frame = NULL;
    AVPacket *packet = NULL;
    struct SwsContext *sws_context = NULL;

    const char *format_name = get_format_name(params.format);
    if (!format_name) {
        DP_error_set("Unknown format");
        result = DP_SAVE_RESULT_UNKNOWN_FORMAT;
        goto cleanup;
    }

    enum AVCodecID codec_id = get_format_codec_id(params.format);
    if (codec_id == AV_CODEC_ID_NONE) {
        DP_error_set("Unknown codec");
        result = DP_SAVE_RESULT_UNKNOWN_FORMAT;
        goto cleanup;
    }

    int output_width = get_format_dimension(params.format, params.width);
    int output_height = get_format_dimension(params.format, params.height);
    double framerate = params.framerate;

    const AVOutputFormat *output_format =
        av_guess_format(format_name, NULL, NULL);
    if (!output_format) {
        DP_error_set("Failed to guess format for '%s'", format_name);
        result = DP_SAVE_RESULT_UNKNOWN_FORMAT;
        goto cleanup;
    }

    const AVCodec *codec = get_codec_encoder(codec_id);
    if (!codec) {
        DP_error_set("Failed to find codec for '%s'", format_name);
        result = DP_SAVE_RESULT_UNKNOWN_FORMAT;
        goto cleanup;
    }

    codec_context = avcodec_alloc_context3(codec);
    if (!codec_context) {
        DP_error_set("Failed to allocate codec context");
        result = DP_SAVE_RESULT_INTERNAL_ERROR;
        goto cleanup;
    }

    codec_context->width =
        get_format_codec_dimension(params.format, output_width);
    codec_context->height =
        get_format_codec_dimension(params.format, output_height);
    codec_context->pix_fmt = get_format_pix_fmt(params.format, false);
    codec_context->framerate = av_d2q(framerate, 1000000000);
    codec_context->time_base = av_inv_q(codec_context->framerate);
    set_format_codec_params(params.format, codec_context);

    int err = avformat_alloc_output_context2(
        &format_context, REMOVE_CONST(output_format), NULL,
        params.destination == DP_SAVE_VIDEO_DESTINATION_PATH
            ? params.destination_param
            : NULL);
    if (err != 0) {
        DP_error_set("Error creating output context: %s", av_err2str(err));
        result = DP_SAVE_RESULT_INTERNAL_ERROR;
        goto cleanup;
    }

    set_format_output_params(params.format, format_context);

    AVStream *stream = avformat_new_stream(format_context, NULL);
    if (!stream) {
        DP_error_set("Failed to create stream");
        result = DP_SAVE_RESULT_INTERNAL_ERROR;
        goto cleanup;
    }

    stream->avg_frame_rate = codec_context->framerate;
    stream->time_base = codec_context->time_base;
    err = avcodec_open2(codec_context, codec, NULL);
    if (err != 0) {
        DP_error_set("Error opening codec: %s", av_err2str(err));
        result = DP_SAVE_RESULT_INTERNAL_ERROR;
        goto cleanup;
    }

    codec_parameters = avcodec_parameters_alloc();
    if (!codec_parameters) {
        DP_error_set("Failed to create codec parameters");
        result = DP_SAVE_RESULT_INTERNAL_ERROR;
        goto cleanup;
    }

    err = avcodec_parameters_from_context(codec_parameters, codec_context);
    if (err != 0) {
        DP_error_set("Error creating codec parameters from context: %s",
                     av_err2str(err));
        result = DP_SAVE_RESULT_INTERNAL_ERROR;
        goto cleanup;
    }

    err = avcodec_parameters_copy(stream->codecpar, codec_parameters);
    avcodec_parameters_free(&codec_parameters);
    if (err != 0) {
        DP_error_set("Error copying parameters: %s", av_err2str(err));
        result = DP_SAVE_RESULT_INTERNAL_ERROR;
        goto cleanup;
    }

    if (format_context->oformat->flags & AVFMT_GLOBALHEADER) {
        format_context->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    const char *filter_desc = get_format_filter(params.format);
    if (filter_desc) {
        const AVFilter *buffersrc_filter = avfilter_get_by_name("buffer");
        if (!buffersrc_filter) {
            DP_error_set("Filter 'buffer' not found");
            result = DP_SAVE_RESULT_INTERNAL_ERROR;
            goto cleanup;
        }

        const AVFilter *buffersink_filter = avfilter_get_by_name("buffersink");
        if (!buffersink_filter) {
            DP_error_set("Filter 'buffersink' not found");
            result = DP_SAVE_RESULT_INTERNAL_ERROR;
            goto cleanup;
        }

        filtered_frame = av_frame_alloc();
        if (!filtered_frame) {
            DP_error_set("Failed to allocate filter frame");
            result = DP_SAVE_RESULT_INTERNAL_ERROR;
            goto cleanup;
        }

        if (params.palette_data) {
            filter_palette_outputs = avfilter_inout_alloc();
            if (!filter_palette_outputs) {
                DP_error_set("Failed to allocate filter palette outputs");
                result = DP_SAVE_RESULT_INTERNAL_ERROR;
                goto cleanup;
            }
        }

        filter_outputs = avfilter_inout_alloc();
        if (!filter_outputs) {
            DP_error_set("Failed to allocate filter outputs");
            result = DP_SAVE_RESULT_INTERNAL_ERROR;
            goto cleanup;
        }

        filter_inputs = avfilter_inout_alloc();
        if (!filter_inputs) {
            DP_error_set("Failed to allocate filter inputs");
            result = DP_SAVE_RESULT_INTERNAL_ERROR;
            goto cleanup;
        }

        graph_context = avfilter_graph_alloc();
        if (!graph_context) {
            DP_error_set("Failed to create filter graph");
            result = DP_SAVE_RESULT_INTERNAL_ERROR;
            goto cleanup;
        }

        char *buffersrc_args = DP_format(
            "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
            output_width, output_height,
            get_format_pix_fmt(params.format, true), stream->time_base.num,
            stream->time_base.den, codec_context->sample_aspect_ratio.num,
            codec_context->sample_aspect_ratio.den);
        err = avfilter_graph_create_filter(&buffersrc_context, buffersrc_filter,
                                           "in", buffersrc_args, NULL,
                                           graph_context);
        DP_free(buffersrc_args);
        if (err < 0) {
            DP_error_set("Error creating filter buffer source: %s",
                         av_err2str(err));
            result = DP_SAVE_RESULT_INTERNAL_ERROR;
            goto cleanup;
        }

        if (params.palette_data) {
            DP_ASSERT(params.palette_size == DP_SAVE_VIDEO_GIF_PALETTE_BYTES);
            char *palettesrc_args = DP_format(
                "video_size=%dx%d:pix_fmt=%d:time_base=1/24:pixel_aspect=0/1",
                DP_SAVE_VIDEO_GIF_PALETTE_DIMENSION,
                DP_SAVE_VIDEO_GIF_PALETTE_DIMENSION, (int)AV_PIX_FMT_BGRA);
            err = avfilter_graph_create_filter(
                &palettesrc_context, buffersrc_filter, "palette",
                palettesrc_args, NULL, graph_context);
            DP_free(palettesrc_args);
            if (err < 0) {
                DP_error_set("Error creating filter palette buffer source: %s",
                             av_err2str(err));
                result = DP_SAVE_RESULT_INTERNAL_ERROR;
                goto cleanup;
            }
        }

        buffersink_context = avfilter_graph_alloc_filter(
            graph_context, buffersink_filter, "out");
        if (!buffersink_context) {
            DP_error_set("Error allocating filter buffer sink");
            result = DP_SAVE_RESULT_INTERNAL_ERROR;
            goto cleanup;
        }

        const enum AVPixelFormat *pix_fmts =
            get_format_filter_pix_fmts(params.format);
#if LIBAVUTIL_VERSION_MAJOR >= 60
        // Option int lists and "pix_fmt" got deprecated and replaced with array
        // options and "pixel_formats".
        unsigned int pix_fmts_count = 0;
        for (int i = 0; pix_fmts[i] != AV_PIX_FMT_NONE; ++i) {
            ++pix_fmts_count;
        }
        err = av_opt_set_array(buffersink_context, "pixel_formats",
                               AV_OPT_SEARCH_CHILDREN | AV_OPT_ARRAY_REPLACE, 0,
                               pix_fmts_count, AV_OPT_TYPE_PIXEL_FMT, pix_fmts);
#else
        // av_opt_set_int_list is a really wonky macro under the hood, relying
        // on implicit conversions that trigger various warnings in that regard.
#    if defined(__GNUC__)
#        pragma GCC diagnostic push
#        pragma GCC diagnostic ignored "-Wconversion"
#        pragma GCC diagnostic ignored "-Wsign-conversion"
#    elif defined(_MSC_VER)
#        pragma warning(push)
#        pragma warning(disable : 4245)
#    endif
        err = av_opt_set_int_list(buffersink_context, "pix_fmts", pix_fmts,
                                  AV_PIX_FMT_NONE, AV_OPT_SEARCH_CHILDREN);
#    if defined(__GNUC__)
#        pragma GCC diagnostic pop
#    elif defined(_MSC_VER)
#        pragma warning(pop)
#    endif
#endif
        if (err < 0) {
            DP_error_set("Error setting buffersink pixel format: %s",
                         av_err2str(err));
            goto cleanup;
        }

        err = avfilter_init_str(buffersink_context, NULL);
        if (err < 0) {
            DP_error_set("Error initializing filter buffer sink: %s",
                         av_err2str(err));
            result = DP_SAVE_RESULT_INTERNAL_ERROR;
            goto cleanup;
        }

        if (filter_palette_outputs) {
            filter_palette_outputs->name = av_strdup("palette");
            filter_palette_outputs->filter_ctx = palettesrc_context;
            filter_palette_outputs->pad_idx = 0;
            filter_palette_outputs->next = NULL;
        }

        filter_outputs->name = av_strdup("in");
        filter_outputs->filter_ctx = buffersrc_context;
        filter_outputs->pad_idx = 0;
        filter_outputs->next = filter_palette_outputs;
        filter_palette_outputs = NULL;

        filter_inputs->name = av_strdup("out");
        filter_inputs->filter_ctx = buffersink_context;
        filter_inputs->pad_idx = 0;
        filter_inputs->next = NULL;

        err = avfilter_graph_parse_ptr(graph_context, filter_desc,
                                       &filter_inputs, &filter_outputs, NULL);
        if (err < 0) {
            DP_error_set("Error parsing filter description '%s': %s",
                         filter_desc, av_err2str(err));
            result = DP_SAVE_RESULT_INTERNAL_ERROR;
            goto cleanup;
        }

        err = avfilter_graph_config(graph_context, NULL);
        if (err < 0) {
            DP_error_set("Invalid filter configuration: %s", av_err2str(err));
            result = DP_SAVE_RESULT_INTERNAL_ERROR;
            goto cleanup;
        }

        avfilter_inout_free(&filter_outputs);
        avfilter_inout_free(&filter_inputs);
    }

    if (params.destination == DP_SAVE_VIDEO_DESTINATION_PATH) {
        output = DP_file_output_new_from_path(params.destination_param);
        if (!output) {
            result = DP_SAVE_RESULT_OPEN_ERROR;
            goto cleanup;
        }
    }
    else {
        output = params.destination_param;
    }

    unsigned char *buffer = av_malloc(BUFFER_SIZE);
    format_context->pb =
        avio_alloc_context(buffer, BUFFER_SIZE, AVIO_FLAG_WRITE, output, NULL,
                           write_avio_packet, seek_avio);
    if (!format_context->pb) {
        DP_error_set("Failed to create IO context");
        result = DP_SAVE_RESULT_OPEN_ERROR;
        av_freep(&buffer);
        goto cleanup;
    }

    format_context->flags |= AVFMT_FLAG_CUSTOM_IO;
    format_context->pb->seekable = AVIO_SEEKABLE_NORMAL;

    err = avformat_write_header(format_context, NULL);
    if (err != 0) {
        DP_error_set("Error writing header: %s", av_err2str(err));
        result = DP_SAVE_RESULT_WRITE_ERROR;
        goto cleanup;
    }

    frame = av_frame_alloc();
    if (!frame) {
        DP_error_set("Failed to allocate frame");
        result = DP_SAVE_RESULT_INTERNAL_ERROR;
        goto cleanup;
    }

    frame->width = output_width;
    frame->height = output_height;
    frame->format = get_format_pix_fmt(params.format, true);

    err = av_frame_get_buffer(frame, 0);
    if (err != 0) {
        DP_error_set("Error getting frame buffer: %s", av_err2str(err));
        result = DP_SAVE_RESULT_INTERNAL_ERROR;
        goto cleanup;
    }

    packet = av_packet_alloc();
    if (!packet) {
        DP_error_set("Failed to allocate packet");
        result = DP_SAVE_RESULT_INTERNAL_ERROR;
        goto cleanup;
    }

    if (!report_progress(params.progress_fn, params.user, 0.0)) {
        result = DP_SAVE_RESULT_CANCEL;
        goto cleanup;
    }

    if (palettesrc_context) {
        palette_frame = av_frame_alloc();
        if (!palette_frame) {
            DP_error_set("Error allocating palette frame");
            result = DP_SAVE_RESULT_INTERNAL_ERROR;
            goto cleanup;
        }

        palette_frame->width = DP_SAVE_VIDEO_GIF_PALETTE_DIMENSION;
        palette_frame->height = DP_SAVE_VIDEO_GIF_PALETTE_DIMENSION;
        palette_frame->format = AV_PIX_FMT_BGRA;
        err = av_frame_get_buffer(palette_frame, 0);
        if (err != 0) {
            DP_error_set("Error getting palette frame buffer: %s",
                         av_err2str(err));
            result = DP_SAVE_RESULT_INTERNAL_ERROR;
            goto cleanup;
        }

        err = av_frame_make_writable(palette_frame);
        if (err != 0) {
            DP_error_set("Error making palette frame writeable: %s",
                         av_err2str(err));
            result = DP_SAVE_RESULT_INTERNAL_ERROR;
            goto cleanup;
        }

        memcpy(palette_frame->data[0], params.palette_data,
               DP_SAVE_VIDEO_GIF_PALETTE_BYTES);
        err = av_buffersrc_add_frame_flags(palettesrc_context, palette_frame,
                                           AV_BUFFERSRC_FLAG_KEEP_REF);
        if (err < 0) {
            DP_error_set("Error feeding palette filter: %s", av_err2str(err));
            result = DP_SAVE_RESULT_INTERNAL_ERROR;
            goto cleanup;
        }

        err = av_buffersrc_add_frame_flags(palettesrc_context, NULL,
                                           AV_BUFFERSRC_FLAG_KEEP_REF);
        if (err < 0) {
            DP_error_set("Error flushing palette filter: %s", av_err2str(err));
            result = DP_SAVE_RESULT_INTERNAL_ERROR;
            goto cleanup;
        }
    }

    frame->pts = 0;
    double last_progress = 0.0;
    DP_SaveVideoNextFrame f = {DP_SAVE_RESULT_SUCCESS, 0, 0, NULL, 0.0, 0};
    while (params.next_frame_fn(params.user, &f)) {
        int input_width = f.width;
        int input_height = f.height;
        if (input_width <= 0 || input_height <= 0 || !f.pixels) {
            DP_error_set("Frame has no image");
            result = DP_SAVE_RESULT_INTERNAL_ERROR;
            goto cleanup;
        }

        if (f.instances < 1) {
            DP_error_set("Frame has %d instances (should be >= 1)",
                         f.instances);
            result = DP_SAVE_RESULT_INTERNAL_ERROR;
            goto cleanup;
        }

        err = av_frame_make_writable(frame);
        if (err != 0) {
            DP_error_set("Error making frame writeable: %s", av_err2str(err));
            result = DP_SAVE_RESULT_INTERNAL_ERROR;
            goto cleanup;
        }

        sws_context = sws_getCachedContext(
            sws_context, input_width, input_height, AV_PIX_FMT_BGRA,
            output_width, output_height, frame->format,
            get_scaling_flags(params.flags, input_width, input_height,
                              output_width, output_height),
            NULL, NULL, NULL);
        if (!sws_context) {
            DP_error_set("Failed to allocate scaling context");
            result = DP_SAVE_RESULT_INTERNAL_ERROR;
            goto cleanup;
        }

        const uint8_t *data = f.pixels;
        const int stride = input_width * 4;
        sws_scale(sws_context, &data, &stride, 0, input_height, frame->data,
                  frame->linesize);

        for (int i = 0; i < f.instances; ++i) {
            result = filter_frame(codec_context, format_context, stream, frame,
                                  packet, buffersrc_context, buffersink_context,
                                  filtered_frame);
            if (result != DP_SAVE_RESULT_SUCCESS) {
                goto cleanup;
            }
            ++frame->pts;
        }

        if (isfinite(f.progress) && f.progress >= last_progress) {
            last_progress = DP_min_double(f.progress, 1.0);
        }

        if (!report_progress(params.progress_fn, params.user,
                             last_progress * 0.97)) {
            result = DP_SAVE_RESULT_CANCEL;
            goto cleanup;
        }
    }

    if (f.result != DP_SAVE_RESULT_SUCCESS) {
        result = f.result;
        goto cleanup;
    }

    av_frame_free(&frame);

    if (!report_progress(params.progress_fn, params.user, 0.98)) {
        result = DP_SAVE_RESULT_CANCEL;
        goto cleanup;
    }

    result =
        filter_frame(codec_context, format_context, stream, NULL, packet,
                     buffersrc_context, buffersink_context, filtered_frame);
    if (result != DP_SAVE_RESULT_SUCCESS) {
        goto cleanup;
    }

    av_packet_free(&packet);

    if (!report_progress(params.progress_fn, params.user, 0.99)) {
        result = DP_SAVE_RESULT_CANCEL;
        goto cleanup;
    }

    err = av_write_trailer(format_context);
    if (err != 0) {
        DP_error_set("Error writing trailer: %s", av_err2str(err));
        result = DP_SAVE_RESULT_WRITE_ERROR;
        goto cleanup;
    }

    if (!report_progress(params.progress_fn, params.user, 1.0)) {
        result = DP_SAVE_RESULT_CANCEL;
        goto cleanup;
    }

    avio_flush(format_context->pb);
    av_freep(&format_context->pb->buffer);
    avio_context_free(&format_context->pb);

    if (!DP_output_flush(output)) {
        result = DP_SAVE_RESULT_WRITE_ERROR;
        goto cleanup;
    }

    if (params.destination == DP_SAVE_VIDEO_DESTINATION_PATH) {
        bool close_ok = DP_output_free(output);
        output = NULL;
        if (!close_ok) {
            result = DP_SAVE_RESULT_WRITE_ERROR;
            goto cleanup;
        }
    }

cleanup:
    sws_freeContext(sws_context);
    if (format_context && format_context->pb) {
        av_freep(&format_context->pb->buffer);
        avio_context_free(&format_context->pb);
    }
    if (params.destination == DP_SAVE_VIDEO_DESTINATION_PATH) {
        DP_output_free_discard(output);
    }
    avfilter_graph_free(&graph_context);
    avfilter_inout_free(&filter_inputs);
    avfilter_inout_free(&filter_outputs);
    avfilter_inout_free(&filter_palette_outputs);
    av_packet_free(&packet);
    av_frame_free(&frame);
    av_frame_free(&filtered_frame);
    av_frame_free(&palette_frame);
    avcodec_parameters_free(&codec_parameters);
    avformat_free_context(format_context);
    avcodec_free_context(&codec_context);
    return result;
}

static void argv_init(DP_Vector *args)
{
    DP_VECTOR_INIT_TYPE(args, const char *, 32);
}

static void argv_dispose(DP_Vector *args)
{
    DP_vector_dispose(args);
}

static void argv_push(DP_Vector *args, const char *s)
{
    DP_VECTOR_PUSH_TYPE(args, const char *, s);
}

static bool push_format_ffmpeg_args(DP_Vector *args, int format,
                                    const char **out_name)
{
    switch (format) {
    case DP_SAVE_VIDEO_FORMAT_MP4_VP9:
        if (args) {
            argv_push(args, "-c:v");
            argv_push(args, "libvpx-vp9");
            argv_push(args, "-pix_fmt");
            argv_push(args, "yuv420p");
            argv_push(args, "-crf");
            argv_push(args, "20");
            argv_push(args, "-b:v");
            argv_push(args, "0");
            argv_push(args, "-an");
            argv_push(args, "-f");
            argv_push(args, "mp4");
        }
        if (out_name) {
            *out_name = "libvpx-vp9";
        }
        return true;
    case DP_SAVE_VIDEO_FORMAT_WEBM_VP8:
        if (args) {
            argv_push(args, "-c:v");
            argv_push(args, "libvpx");
            argv_push(args, "-pix_fmt");
            argv_push(args, "yuv420p");
            argv_push(args, "-crf");
            argv_push(args, "15");
            argv_push(args, "-b:v");
            argv_push(args, "50M");
            argv_push(args, "-an");
            argv_push(args, "-f");
            argv_push(args, "webm");
        }
        if (out_name) {
            *out_name = "libvpx";
        }
        return true;
    case DP_SAVE_VIDEO_FORMAT_WEBP:
        if (args) {
            argv_push(args, "-c:v");
            argv_push(args, "libwebp");
            argv_push(args, "-lossless");
            argv_push(args, "1");
            argv_push(args, "-preset");
            argv_push(args, "drawing");
            argv_push(args, "-quality");
            argv_push(args, "100");
            argv_push(args, "-loop");
            argv_push(args, "0");
            argv_push(args, "-f");
            argv_push(args, "webp");
        }
        if (out_name) {
            *out_name = "libwebp";
        }
        return true;
    case DP_SAVE_VIDEO_FORMAT_MP4_H264:
        if (args) {
            argv_push(args, "-c:v");
            argv_push(args, "libx264");
            argv_push(args, "-pix_fmt");
            argv_push(args, "yuv420p");
            argv_push(args, "-crf");
            argv_push(args, "19");
            argv_push(args, "-tune");
            argv_push(args, "animation");
            argv_push(args, "-an");
            argv_push(args, "-f");
            argv_push(args, "mp4");
        }
        if (out_name) {
            *out_name = "libx264";
        }
        return true;
    case DP_SAVE_VIDEO_FORMAT_MP4_AV1:
        if (args) {
            argv_push(args, "-c:v");
            argv_push(args, "libsvtav1");
            argv_push(args, "-pix_fmt");
            argv_push(args, "yuv420p");
            argv_push(args, "-crf");
            argv_push(args, "23");
            argv_push(args, "-b:v");
            argv_push(args, "0");
            argv_push(args, "-an");
            argv_push(args, "-f");
            argv_push(args, "mp4");
        }
        if (out_name) {
            *out_name = "libsvtav1";
        }
        return true;
    case DP_SAVE_VIDEO_FORMAT_APNG:
        if (args) {
            argv_push(args, "-c:v");
            argv_push(args, "apng");
            argv_push(args, "-plays");
            argv_push(args, "0");
            argv_push(args, "-f");
            argv_push(args, "apng");
        }
        if (out_name) {
            *out_name = "apng";
        }
        return true;
    default:
        return false;
    }
}


struct DP_SaveVideoSupport {
    unsigned int type_flags;
    int count;
    DP_SaveVideoSupportEntry entries[];
};

static void get_save_video_format_support_libav(int format, DP_Vector *vec)
{
    enum AVCodecID codec_id = get_format_codec_id(format);
    if (codec_id != AV_CODEC_ID_NONE) {
        const AVCodec *encoder = avcodec_find_encoder(codec_id);
        if (encoder) {
            DP_SaveVideoSupportEntry entry = {DP_SAVE_VIDEO_ENCODER_TYPE_LIBAV,
                                              encoder->name};
            DP_VECTOR_PUSH_TYPE(vec, DP_SaveVideoSupportEntry, entry);
        }
    }
}

static void get_save_video_format_support_ffmpeg(int format, DP_Vector *vec)
{
    const char *name;
    if (DP_process_supported()
        && push_format_ffmpeg_args(NULL, format, &name)) {
        DP_SaveVideoSupportEntry entry = {DP_SAVE_VIDEO_ENCODER_TYPE_FFMPEG,
                                          name};
        DP_VECTOR_PUSH_TYPE(vec, DP_SaveVideoSupportEntry, entry);
    }
}

#ifdef DP_ANDROID_VIDEO_ENCODER
static void add_android_support_save_video_support_entry(void *user,
                                                         const char *name,
                                                         bool hardware)
{
    DP_SaveVideoSupportEntry entry = {
        hardware ? DP_SAVE_VIDEO_ENCODER_TYPE_ANDROID_HARDWARE
                 : DP_SAVE_VIDEO_ENCODER_TYPE_ANDROID_SOFTWARE,
        DP_strdup(name)};
    DP_VECTOR_PUSH_TYPE(user, DP_SaveVideoSupportEntry, entry);
}

static void get_save_video_format_support_android(int format, DP_Vector *vec)
{
    DP_android_video_encoder_format_support(
        format, add_android_support_save_video_support_entry, vec);
}
#endif

#ifdef DP_WINDOWS_VIDEO_ENCODER
static void get_save_video_format_support_windows(int format, DP_Vector *vec)
{
    if (DP_windows_video_encoder_format_support(format)) {
        DP_SaveVideoSupportEntry entry = {DP_SAVE_VIDEO_ENCODER_TYPE_WINDOWS,
                                          "MediaFoundation"};
        DP_VECTOR_PUSH_TYPE(vec, DP_SaveVideoSupportEntry, entry);
    }
}
#endif

static DP_SaveVideoSupport *get_save_video_format_support(int format)
{
    DP_Vector vec;
    DP_VECTOR_INIT_TYPE(&vec, DP_SaveVideoSupport, 8);

    get_save_video_format_support_libav(format, &vec);
    get_save_video_format_support_ffmpeg(format, &vec);
#ifdef DP_ANDROID_VIDEO_ENCODER
    get_save_video_format_support_android(format, &vec);
#endif
#ifdef DP_WINDOWS_VIDEO_ENCODER
    get_save_video_format_support_windows(format, &vec);
#endif

    DP_SaveVideoSupport *support =
        malloc(DP_FLEX_SIZEOF(DP_SaveVideoSupport, entries, vec.used));

    support->type_flags = 0u;
    support->count = DP_size_to_int(vec.used);
    for (size_t i = 0; i < vec.used; ++i) {
        support->entries[i] =
            DP_VECTOR_AT_TYPE(&vec, DP_SaveVideoSupportEntry, i);
        support->type_flags |= 1u << ((unsigned int)support->entries[i].type);
    }

    DP_vector_dispose(&vec);
    return support;
}

DP_SaveVideoSupport *DP_save_video_format_support(int format)
{
    static DP_SaveVideoSupport *supports[DP_SAVE_VIDEO_FORMAT_LAST + 1];
    if (format >= 0 && format <= DP_SAVE_VIDEO_FORMAT_LAST) {
        DP_SaveVideoSupport *support = supports[format];
        if (!support) {
            support = get_save_video_format_support(format);
            supports[format] = support;
        }
        return support;
    }
    else {
        DP_warn("Video format %d out of bounds", format);
        static DP_SaveVideoSupport null_support;
        return &null_support;
    }
}

int DP_save_video_support_count(DP_SaveVideoSupport *support)
{
    if (support) {
        return support->count;
    }
    else {
        return 0;
    }
}

const DP_SaveVideoSupportEntry *
DP_save_video_support_entry(DP_SaveVideoSupport *support, int index)
{
    if (support && index >= 0 && index < support->count) {
        return &support->entries[index];
    }
    else {
        return NULL;
    }
}

bool DP_save_video_format_supported_ffmpeg(int format)
{
    return DP_save_video_format_support(format)->type_flags
         & (1u << ((unsigned int)DP_SAVE_VIDEO_ENCODER_TYPE_FFMPEG));
}

bool DP_save_video_format_supported_non_ffmpeg(int format)
{
    return DP_save_video_format_support(format)->type_flags
         & ~(1u << ((unsigned int)DP_SAVE_VIDEO_ENCODER_TYPE_FFMPEG));
}


static DP_SaveResult save_video_ffmpeg(DP_SaveVideoParams params)
{
    DP_SaveResult result = DP_SAVE_RESULT_SUCCESS;
    DP_Vector args = DP_VECTOR_NULL;
    char *dimensions_arg = NULL;
    char *framerate_arg = NULL;
    DP_Process *process = NULL;
    struct SwsContext *sws_context = NULL;
    unsigned char *output_buffer = NULL;

    if (!DP_process_supported()) {
        DP_error_set("Process spawning not supported");
        result = DP_SAVE_RESULT_BAD_ARGUMENTS;
        goto cleanup;
    }

    int output_width = get_format_dimension(params.format, params.width);
    int output_height = get_format_dimension(params.format, params.height);

    dimensions_arg = DP_format("%dx%d", output_width, output_height);
    framerate_arg = DP_format("%f", params.framerate);

    DP_SaveVideoFfmpegParams *ffmpeg = params.destination_param;

    argv_init(&args);
    argv_push(&args, ffmpeg->program);
    argv_push(&args, "-f");
    argv_push(&args, "rawvideo");
    argv_push(&args, "-pix_fmt");
    argv_push(&args, "rgb32");
    argv_push(&args, "-s:v");
    argv_push(&args, dimensions_arg);
    argv_push(&args, "-r");
    argv_push(&args, framerate_arg);
    argv_push(&args, "-i");
    argv_push(&args, "pipe:0");

    if (!push_format_ffmpeg_args(&args, params.format, NULL)) {
        DP_error_set("Unhandled format for ffmpeg");
        result = DP_SAVE_RESULT_UNKNOWN_FORMAT;
        goto cleanup;
    }

    if (ffmpeg->custom_args) {
        for (int i = 0; ffmpeg->custom_args[i]; ++i) {
            argv_push(&args, ffmpeg->custom_args[i]);
        }
    }

    argv_push(&args, "-y");
    argv_push(&args, ffmpeg->output);

    process = DP_process_new(DP_size_to_int(args.used), args.elements);
    if (!process) {
        result = DP_SAVE_RESULT_INTERNAL_ERROR;
        goto cleanup;
    }

    double last_progress = 0.0;
    DP_SaveVideoNextFrame f = {DP_SAVE_RESULT_SUCCESS, 0, 0, NULL, 0.0, 0};
    size_t frame_size = DP_int_to_size(output_width)
                      * DP_int_to_size(output_height) * (size_t)4;
    while (params.next_frame_fn(params.user, &f)) {
        int input_width = f.width;
        int input_height = f.height;
        if (input_width <= 0 || input_height <= 0 || !f.pixels) {
            DP_error_set("Frame has no image");
            result = DP_SAVE_RESULT_INTERNAL_ERROR;
            goto cleanup;
        }

        if (f.instances < 1) {
            DP_error_set("Frame has %d instances (should be >= 1)",
                         f.instances);
            result = DP_SAVE_RESULT_INTERNAL_ERROR;
            goto cleanup;
        }

        const unsigned char *frame;
        if (input_width == output_width && input_height == output_height) {
            frame = f.pixels;
        }
        else {
            sws_context = sws_getCachedContext(
                sws_context, input_width, input_height, AV_PIX_FMT_BGRA,
                output_width, output_height, AV_PIX_FMT_BGRA,
                get_scaling_flags(params.flags, input_width, input_height,
                                  output_width, output_height),
                NULL, NULL, NULL);
            if (!sws_context) {
                DP_error_set("Failed to allocate scaling context");
                result = DP_SAVE_RESULT_INTERNAL_ERROR;
                goto cleanup;
            }

            if (!output_buffer) {
                output_buffer = DP_malloc(frame_size);
            }

            DP_image_swscale_pixels_into(sws_context, f.pixels, f.width,
                                         f.height, output_buffer, output_width,
                                         output_height);
            frame = output_buffer;
        }

        for (int i = 0; i < f.instances; ++i) {
            if (!DP_process_write(process, frame_size, frame)) {
                result = DP_SAVE_RESULT_WRITE_ERROR;
                goto cleanup;
            }
        }

        if (isfinite(f.progress) && f.progress >= last_progress) {
            last_progress = DP_min_double(f.progress, 1.0);
        }

        if (!report_progress(params.progress_fn, params.user,
                             last_progress * 0.97)) {
            result = DP_SAVE_RESULT_CANCEL;
            goto cleanup;
        }
    }

    if (f.result != DP_SAVE_RESULT_SUCCESS) {
        result = f.result;
        goto cleanup;
    }

    DP_process_close(process);
    do {
        if (!report_progress(params.progress_fn, params.user, 0.99)) {
            result = DP_SAVE_RESULT_CANCEL;
            goto cleanup;
        }
    } while (DP_process_wait(process, 1000));

    DP_process_free_close_wait(process);
    process = NULL;

    if (!report_progress(params.progress_fn, params.user, 1.0)) {
        result = DP_SAVE_RESULT_CANCEL;
        goto cleanup;
    }

cleanup:
    DP_free(output_buffer);
    sws_freeContext(sws_context);
    if (process) {
        DP_process_close(process);
        if (!DP_process_wait(process, 10000)) {
            DP_process_kill(process);
        }
        DP_process_free_close_wait(process);
    }
    argv_dispose(&args);
    DP_free(framerate_arg);
    DP_free(dimensions_arg);
    return result;
}

#if defined(DP_ANDROID_VIDEO_ENCODER) || defined(DP_WINDOWS_VIDEO_ENCODER)
typedef struct DP_SaveVideoProgress {
    double progress;
    DP_SaveProgressFn progress_fn;
    void *user;
} DP_SaveVideoProgress;

static void save_video_progress_frame(DP_SaveVideoProgress *vp, double progress,
                                      double max_progress)
{
    if (isfinite(progress) && progress >= vp->progress) {
        vp->progress = DP_min_double(progress, 1.0) * max_progress;
    }
}

static bool save_video_progress_report(DP_SaveVideoProgress *vp,
                                       DP_SaveResult *out_save_result)
{
    if (report_progress(vp->progress_fn, vp->user, vp->progress)) {
        return true;
    }
    else {
        *out_save_result = DP_SAVE_RESULT_CANCEL;
        return false;
    }
}
#endif

#ifdef DP_ANDROID_VIDEO_ENCODER
#    define ANDROID_PREPARE_OK          0
#    define ANDROID_DRAIN_END_OF_STREAM (-1)
#    define ANDROID_DRAIN_ERROR         (-2)
#    define ANDROID_PREPARE_ERROR       (-3)

static int android_drain(DP_AndroidVideoEncoder *ave, long long initial_timeout,
                         DP_SaveVideoProgress *vp,
                         DP_SaveResult *out_save_result)
{
    int count = 0;
    long long timeout = initial_timeout;
    while (true) {
        if (!save_video_progress_report(vp, out_save_result)) {
            return ANDROID_DRAIN_ERROR;
        }

        int ave_result = DP_android_video_encoder_drain(ave, timeout);
        if (ave_result == DP_ANDROID_VIDEO_ENCODER_STATUS_OK) {
            timeout = 0LL; // Keep draining whatever is there without waiting.
            ++count;
        }
        else if (ave_result == DP_ANDROID_VIDEO_ENCODER_STATUS_TIMEOUT) {
            break;
        }
        else if (ave_result == DP_ANDROID_VIDEO_ENCODER_STATUS_END_OF_STREAM) {
            return ANDROID_DRAIN_END_OF_STREAM;
        }
        else {
            *out_save_result = DP_SAVE_RESULT_INTERNAL_ERROR;
            return ANDROID_DRAIN_ERROR;
        }
    }
    return count;
}

static int android_prepare(DP_AndroidVideoEncoder *ave,
                           DP_SaveVideoProgress *vp,
                           DP_SaveResult *out_save_result)
{
    while (true) {
        if (!save_video_progress_report(vp, out_save_result)) {
            return ANDROID_PREPARE_ERROR;
        }

        int prepare_result = DP_android_video_encoder_prepare(ave, 100000LL);
        if (prepare_result == DP_ANDROID_VIDEO_ENCODER_STATUS_OK) {
            break;
        }
        else if (prepare_result == DP_ANDROID_VIDEO_ENCODER_STATUS_TIMEOUT) {
            int drain_result = android_drain(ave, 0LL, vp, out_save_result);
            if (drain_result == ANDROID_DRAIN_END_OF_STREAM) {
                DP_error_set("Unexpected end of stream");
                *out_save_result = DP_SAVE_RESULT_INTERNAL_ERROR;
                return ANDROID_DRAIN_END_OF_STREAM;
            }
            else if (drain_result < 0) {
                return drain_result;
            }
        }
        else {
            return ANDROID_PREPARE_ERROR;
        }
    }
    return 0;
}
#endif

static DP_SaveResult save_video_android(DP_SaveVideoParams params)
{
#ifdef DP_ANDROID_VIDEO_ENCODER
    DP_SaveResult result = DP_SAVE_RESULT_SUCCESS;
    DP_AndroidVideoEncoder *ave = NULL;
    struct SwsContext *sws_context = NULL;
    uint8_t *img_buffers[4];
    int img_linesizes[4];
    enum AVPixelFormat img_format = AV_PIX_FMT_NONE;

    {
        const DP_SaveVideoAndroidParams *svap = params.destination_param;
        ave = DP_android_video_encoder_new((DP_AndroidVideoEncoderParams){
            svap->output,
            svap->temp,
            svap->encoder,
            params.framerate,
            params.format,
            params.width,
            params.height,
        });
    }
    if (!ave) {
        result = DP_SAVE_RESULT_OPEN_ERROR;
        goto cleanup;
    }

    int ave_result = DP_android_video_encoder_start(ave);
    if (ave_result != DP_ANDROID_VIDEO_ENCODER_STATUS_OK) {
        result = DP_SAVE_RESULT_INTERNAL_ERROR;
        goto cleanup;
    }

    int output_width = get_format_dimension(params.format, params.width);
    int output_height = get_format_dimension(params.format, params.height);

    DP_SaveVideoProgress vp = {0.0, params.progress_fn, params.user};
    DP_SaveVideoNextFrame f = {DP_SAVE_RESULT_SUCCESS, 0, 0, NULL, 0.0, 0};
    while (params.next_frame_fn(params.user, &f)) {
        int input_width = f.width;
        int input_height = f.height;
        if (input_width <= 0 || input_height <= 0 || !f.pixels) {
            DP_error_set("Frame has no image");
            result = DP_SAVE_RESULT_INTERNAL_ERROR;
            goto cleanup;
        }

        if (f.instances < 1) {
            DP_error_set("Frame has %d instances (should be >= 1)",
                         f.instances);
            result = DP_SAVE_RESULT_INTERNAL_ERROR;
            goto cleanup;
        }

        save_video_progress_frame(&vp, f.progress, 0.97);

        for (int i = 0; i < f.instances; ++i) {
            if (android_prepare(ave, &vp, &result) != ANDROID_PREPARE_OK) {
                goto cleanup;
            }

            DP_AndroidVideoEncoderImage image;
            if (!DP_android_video_encoder_image(ave, &image)) {
                result = DP_SAVE_RESULT_INTERNAL_ERROR;
                goto cleanup;
            }

            uint8_t *dst_buffers[4] = {NULL, NULL, NULL, NULL};
            int dst_linesizes[4] = {0, 0, 0, 0};

            // Android has hardware-based encoding, which has a "flexible" YUV
            // format. It either has everything in its own plane, which means
            // the U and V values are right next to each other with a pixel
            // stride of 1, or the UV values as a combined plane, which means
            // the pixel stride of each individual value is 2.
            enum AVPixelFormat output_pixel_format;
            if (image.pixel_stride_u == 1 && image.pixel_stride_v == 1) {
                output_pixel_format = AV_PIX_FMT_YUV420P;
                dst_buffers[0] = image.buffer_y;
                dst_buffers[1] = image.buffer_u;
                dst_buffers[2] = image.buffer_v;
                dst_linesizes[0] = image.row_stride_y;
                dst_linesizes[1] = image.row_stride_u;
                dst_linesizes[2] = image.row_stride_v;
            }
            else if (image.pixel_stride_u == 2 && image.pixel_stride_v == 2
                     && image.buffer_u + 1 == image.buffer_v) {
                output_pixel_format = AV_PIX_FMT_NV12;
                dst_buffers[0] = image.buffer_y;
                dst_buffers[1] = image.buffer_u;
                dst_linesizes[0] = image.row_stride_y;
                dst_linesizes[1] = image.row_stride_u;
            }
            else if (image.pixel_stride_u == 2 && image.pixel_stride_v == 2
                     && image.buffer_v + 1 == image.buffer_u) {
                output_pixel_format = AV_PIX_FMT_NV21;
                dst_buffers[0] = image.buffer_y;
                dst_buffers[1] = image.buffer_v;
                dst_linesizes[0] = image.row_stride_y;
                dst_linesizes[1] = image.row_stride_v;
            }
            else {
                DP_error_set("Unknown pixel format with u=%p/%d v=%p/%d",
                             (void *)image.buffer_u, image.pixel_stride_u,
                             (void *)image.buffer_y, image.pixel_stride_v);
                result = DP_SAVE_RESULT_INTERNAL_ERROR;
                goto cleanup;
            }

            sws_context = sws_getCachedContext(
                sws_context, input_width, input_height, AV_PIX_FMT_BGRA,
                output_width, output_height, output_pixel_format,
                get_scaling_flags(params.flags, input_width, input_height,
                                  output_width, output_height),
                NULL, NULL, NULL);
            if (!sws_context) {
                DP_error_set("Failed to allocate scaling context");
                result = DP_SAVE_RESULT_INTERNAL_ERROR;
                goto cleanup;
            }

            if (f.instances == 1) {
                // Just a single frame, scale it into the native buffer.
                const uint8_t *src_buffers[] = {f.pixels, NULL, NULL, NULL};
                const int src_linesizes[] = {f.width * 4, 0, 0, 0};
                sws_scale(sws_context, src_buffers, src_linesizes, 0, f.height,
                          dst_buffers, dst_linesizes);
            }
            else {
                // Repeated frame, scale it into an intermediate buffer, then
                // copy it over to the native one for each instance.
                if (i == 0 || img_format != output_pixel_format) {
                    if (img_format != output_pixel_format) {
                        // The Android encoder really shouldn't be changing
                        // pixel formats along the way, but just in case.
                        if (img_format != AV_PIX_FMT_NONE) {
                            DP_warn("Encoder changed pixel format from %d to "
                                    "%d, reallocating image",
                                    (int)img_format, (int)output_pixel_format);
                            img_format = AV_PIX_FMT_NONE;
                            av_freep(&img_buffers[0]);
                        }

                        int img_result = av_image_alloc(
                            img_buffers, img_linesizes, output_width,
                            output_height, output_pixel_format, 32);
                        if (img_result < 0) {
                            DP_error_set("Error %d allocating image buffer",
                                         img_result);
                            result = DP_SAVE_RESULT_INTERNAL_ERROR;
                            goto cleanup;
                        }
                        img_format = output_pixel_format;
                    }

                    const uint8_t *src_slice[] = {f.pixels, NULL, NULL, NULL};
                    const int src_stride[] = {f.width * 4, 0, 0, 0};
                    sws_scale(sws_context, src_slice, src_stride, 0, f.height,
                              img_buffers, img_linesizes);
                }

                av_image_copy2(dst_buffers, dst_linesizes, img_buffers,
                               img_linesizes, output_pixel_format, output_width,
                               output_height);
            }

            ave_result = DP_android_video_encoder_commit(ave);
            if (ave_result != DP_ANDROID_VIDEO_ENCODER_STATUS_OK) {
                result = DP_SAVE_RESULT_WRITE_ERROR;
                goto cleanup;
            }

            if (!save_video_progress_report(&vp, &result)) {
                goto cleanup;
            }
        }
    }

    if (f.result != DP_SAVE_RESULT_SUCCESS) {
        result = f.result;
        goto cleanup;
    }

    vp.progress = 0.98;
    if (!save_video_progress_report(&vp, &result)) {
        goto cleanup;
    }

    if (img_format != AV_PIX_FMT_NONE) {
        img_format = AV_PIX_FMT_NONE;
        av_freep(&img_buffers[0]);
    }
    sws_freeContext(sws_context);
    sws_context = NULL;

    if (android_prepare(ave, &vp, &result) != ANDROID_PREPARE_OK) {
        goto cleanup;
    }

    ave_result = DP_android_video_encoder_finish(ave);
    if (ave_result != DP_ANDROID_VIDEO_ENCODER_STATUS_OK) {
        result = DP_SAVE_RESULT_WRITE_ERROR;
        goto cleanup;
    }

    vp.progress = 0.99;
    while (true) {
        int drain_result = android_drain(ave, 1000000LL, &vp, &result);
        if (drain_result == ANDROID_DRAIN_END_OF_STREAM) {
            break;
        }
        else if (drain_result < 0) {
            goto cleanup;
        }
    }

    vp.progress = 1.0;
    if (!save_video_progress_report(&vp, &result)) {
        goto cleanup;
    }

    ave_result = DP_android_video_encoder_close(ave);
    if (ave_result != DP_ANDROID_VIDEO_ENCODER_STATUS_OK) {
        result = DP_SAVE_RESULT_WRITE_ERROR;
        goto cleanup;
    }

cleanup:
    if (img_format != AV_PIX_FMT_NONE) {
        av_freep(&img_buffers[0]);
    }
    sws_freeContext(sws_context);
    DP_android_video_encoder_free(ave);
    return result;
#else
    (void)params;
    DP_error_set("Android video encoder not supported");
    return DP_SAVE_RESULT_BAD_ARGUMENTS;
#endif
}

static DP_SaveResult save_video_windows(DP_SaveVideoParams params)
{
#ifdef DP_WINDOWS_VIDEO_ENCODER
    DP_SaveResult result = DP_SAVE_RESULT_SUCCESS;
    DP_WindowsVideoEncoder *wve = NULL;
    struct SwsContext *sws_context = NULL;
    uint8_t *img_buffers[4];
    int img_linesizes[4];
    bool img_allocated = false;

    int output_width = get_format_dimension(params.format, params.width);
    int output_height = get_format_dimension(params.format, params.height);

    int frame_size = av_image_get_buffer_size(AV_PIX_FMT_NV12, output_width,
                                              output_height, 1);
    if (frame_size <= 0) {
        DP_error_set("Error %d getting frame size", frame_size);
        return DP_SAVE_RESULT_INTERNAL_ERROR;
    }

    {
        AVRational framerate = av_d2q(params.framerate, 1000000000);
        wve = DP_windows_video_encoder_new((DP_WindowsVideoEncoderParams){
            params.destination_param,
            framerate.num,
            framerate.den,
            output_width,
            output_height,
            frame_size,
        });
    }
    if (!wve) {
        result = DP_SAVE_RESULT_OPEN_ERROR;
        goto cleanup;
    }

    if (!DP_windows_video_encoder_start(wve)) {
        result = DP_SAVE_RESULT_INTERNAL_ERROR;
        goto cleanup;
    }

    DP_SaveVideoProgress vp = {0.0, params.progress_fn, params.user};
    DP_SaveVideoNextFrame f = {DP_SAVE_RESULT_SUCCESS, 0, 0, NULL, 0.0, 0};
    while (params.next_frame_fn(params.user, &f)) {
        int input_width = f.width;
        int input_height = f.height;
        if (input_width <= 0 || input_height <= 0 || !f.pixels) {
            DP_error_set("Frame has no image");
            result = DP_SAVE_RESULT_INTERNAL_ERROR;
            goto cleanup;
        }

        if (f.instances < 1) {
            DP_error_set("Frame has %d instances (should be >= 1)",
                         f.instances);
            result = DP_SAVE_RESULT_INTERNAL_ERROR;
            goto cleanup;
        }

        save_video_progress_frame(&vp, f.progress, 0.98);

        for (int i = 0; i < f.instances; ++i) {
            DP_WindowsVideoEncoderFrame dst;
            if (!DP_windows_video_encoder_prepare(wve, &dst)) {
                result = DP_SAVE_RESULT_INTERNAL_ERROR;
                goto cleanup;
            }

            sws_context = sws_getCachedContext(
                sws_context, input_width, input_height, AV_PIX_FMT_BGRA,
                output_width, output_height, AV_PIX_FMT_NV12,
                get_scaling_flags(params.flags, input_width, input_height,
                                  output_width, output_height),
                NULL, NULL, NULL);
            if (!sws_context) {
                DP_error_set("Failed to allocate scaling context");
                result = DP_SAVE_RESULT_INTERNAL_ERROR;
                goto cleanup;
            }

            if (f.instances == 1) {
                // Just a single frame, scale it into the native buffer.
                const uint8_t *src_buffers[] = {f.pixels, NULL, NULL, NULL};
                const int src_linesizes[] = {f.width * 4, 0, 0, 0};
                sws_scale(sws_context, src_buffers, src_linesizes, 0, f.height,
                          dst.buffers, dst.linesizes);
            }
            else {
                // Repeated frame, scale it into an intermediate buffer, then
                // copy it over to the native one for each instance.
                if (i == 0) {
                    if (!img_allocated) {
                        int img_result = av_image_alloc(
                            img_buffers, img_linesizes, output_width,
                            output_height, AV_PIX_FMT_NV12, 32);
                        if (img_result < 0) {
                            DP_error_set("Error %d allocating image buffer",
                                         img_result);
                            result = DP_SAVE_RESULT_INTERNAL_ERROR;
                            goto cleanup;
                        }
                        img_allocated = true;
                    }

                    const uint8_t *src_slice[] = {f.pixels, NULL, NULL, NULL};
                    const int src_stride[] = {f.width * 4, 0, 0, 0};
                    sws_scale(sws_context, src_slice, src_stride, 0, f.height,
                              img_buffers, img_linesizes);
                }

                av_image_copy2(dst.buffers, dst.linesizes, img_buffers,
                               img_linesizes, AV_PIX_FMT_NV12, output_width,
                               output_height);
            }

            if (!DP_windows_video_encoder_commit(wve)) {
                result = DP_SAVE_RESULT_WRITE_ERROR;
                goto cleanup;
            }

            if (!save_video_progress_report(&vp, &result)) {
                goto cleanup;
            }
        }
    }

    if (f.result != DP_SAVE_RESULT_SUCCESS) {
        result = f.result;
        goto cleanup;
    }

    vp.progress = 0.99;
    if (!save_video_progress_report(&vp, &result)) {
        goto cleanup;
    }

    if (img_allocated) {
        img_allocated = false;
        av_freep(&img_buffers[0]);
    }
    sws_freeContext(sws_context);
    sws_context = NULL;

    if (!DP_windows_video_encoder_finish(wve)) {
        result = DP_SAVE_RESULT_WRITE_ERROR;
        goto cleanup;
    }

    vp.progress = 1.0;
    if (!save_video_progress_report(&vp, &result)) {
        goto cleanup;
    }

cleanup:
    if (img_allocated) {
        av_freep(&img_buffers[0]);
    }
    sws_freeContext(sws_context);
    DP_windows_video_encoder_free(wve);
    return result;
#else
    (void)params;
    DP_error_set("Windows video encoder not supported");
    return DP_SAVE_RESULT_BAD_ARGUMENTS;
#endif
}

DP_SaveResult DP_save_video(DP_SaveVideoParams params)
{
    bool params_ok =
        is_destination_ok(params.destination, params.destination_param)
        && params.width > 0 && params.height > 0;
    if (!params_ok) {
        DP_error_set("Invalid arguments");
        return DP_SAVE_RESULT_BAD_ARGUMENTS;
    }

    if (!check_format_dimensions(params.width, params.height, params.format)) {
        return DP_SAVE_RESULT_BAD_DIMENSIONS;
    }

    if (params.palette_data
        && params.palette_size != DP_SAVE_VIDEO_GIF_PALETTE_BYTES) {
        DP_error_set("Incorrect palette size, got %zu, should be %zu",
                     params.palette_size, DP_SAVE_VIDEO_GIF_PALETTE_BYTES);
        return DP_SAVE_RESULT_BAD_ARGUMENTS;
    }

    if (params.framerate <= 0.0 || !isfinite(params.framerate)) {
        params.framerate = 24.0;
    }

    switch (params.destination) {
    case DP_SAVE_VIDEO_DESTINATION_FFMPEG:
        return save_video_ffmpeg(params);
    case DP_SAVE_VIDEO_DESTINATION_ANDROID:
        return save_video_android(params);
    case DP_SAVE_VIDEO_DESTINATION_WINDOWS:
        return save_video_windows(params);
    default:
        return save_video_libav(params);
    }
}

struct DP_SaveAnimationVideoContext {
    DP_CanvasState *cs;
    DP_Image *img;
    DP_SaveProgressFn progress_fn;
    void *progress_user;
    DP_ViewModeBuffer vmb;
    DP_Rect crop;
    const int *frame_indexes;
    int frame_index_count;
    int loops;
    int frames_to_do;
    int frames_done;
    unsigned int flat_image_flags;
    int loop_index;
    int i;
};

static bool save_animation_video_next_frame(void *user,
                                            DP_SaveVideoNextFrame *f)
{
    struct DP_SaveAnimationVideoContext *c = user;

    if (c->i >= c->frame_index_count) {
        ++c->loop_index;
        c->i = 0;
    }

    if (c->loop_index >= c->loops) {
        f->result = DP_SAVE_RESULT_SUCCESS;
        return false;
    }

    f->instances = 1;
    while (c->i < c->frame_index_count - 1
           && DP_canvas_state_same_frame(c->cs, c->frame_indexes[c->i],
                                         c->frame_indexes[c->i + 1])) {
        ++c->i;
        ++f->instances;
    }

    DP_ViewModeFilter vmf = DP_view_mode_filter_make_frame_render(
        &c->vmb, c->cs, c->frame_indexes[c->i - f->instances + 1]);
    if (!DP_canvas_state_into_flat_image(c->cs, c->flat_image_flags, &c->crop,
                                         &vmf, &c->img)) {
        f->result = DP_SAVE_RESULT_INTERNAL_ERROR;
        return false;
    }

    ++c->i;
    c->frames_done += f->instances;
    f->progress =
        DP_int_to_double(c->frames_done) / DP_int_to_double(c->frames_to_do);

    f->result = DP_SAVE_RESULT_SUCCESS;
    f->pixels = DP_image_pixels(c->img);
    f->width = DP_image_width(c->img);
    f->height = DP_image_height(c->img);
    return true;
}

static bool save_animation_progress(void *user, double progress)
{
    struct DP_SaveAnimationVideoContext *c = user;
    return c->progress_fn(c->progress_user, progress);
}

DP_SaveResult DP_save_animation_video(DP_SaveAnimationVideoParams params)
{
    DP_Rect crop;
    bool params_ok = params.cs && params.frame_indexes
                  && params.frame_index_count > 0
                  && DP_rect_valid(crop = get_crop(params.cs, params.area));
    if (!params_ok) {
        DP_error_set("Invalid arguments");
        return DP_SAVE_RESULT_BAD_ARGUMENTS;
    }

    struct DP_SaveAnimationVideoContext c;
    c.cs = params.cs;
    c.img = NULL;
    c.progress_fn = params.progress_fn;
    c.progress_user = params.user;
    DP_view_mode_buffer_init(&c.vmb);
    c.crop = crop;
    c.frame_indexes = params.frame_indexes;
    c.frame_index_count = params.frame_index_count;
    c.loops = get_format_loops(params.format, params.loops);
    c.frames_to_do = params.frame_index_count * c.loops;
    c.frames_done = 0;
    c.flat_image_flags = get_format_flat_image_flags(params.format);
    c.loop_index = -1;
    c.i = params.frame_index_count;

    double framerate = params.framerate <= 0.0 || !isfinite(params.framerate)
                         ? DP_canvas_state_effective_framerate(params.cs)
                         : params.framerate;

    DP_SaveResult result = DP_save_video((DP_SaveVideoParams){
        params.destination, params.destination_param, params.palette_data,
        params.palette_size, params.flags, params.format, params.width,
        params.height, framerate, save_animation_video_next_frame,
        params.progress_fn ? save_animation_progress : NULL, &c});

    DP_image_free(c.img);
    DP_view_mode_buffer_dispose(&c.vmb);
    return result;
}


struct DP_GifProgressParams {
    bool palette;
    DP_SaveProgressFn progress_fn;
    void *user;
};

static bool on_gif_progress_palette(void *user, double progress)
{
    DP_SaveAnimationGifParams *params = user;
    DP_SaveProgressFn progress_fn = params->progress_fn;
    return progress_fn ? progress_fn(params->user, progress * 0.49) : true;
}

static bool on_gif_progress_gif(void *user, double progress)
{
    DP_SaveAnimationGifParams *params = user;
    DP_SaveProgressFn progress_fn = params->progress_fn;
    return progress_fn ? progress_fn(params->user, progress * 0.5 + 0.5) : true;
}

DP_SaveResult DP_save_animation_gif(DP_SaveAnimationGifParams params)
{
    DP_Rect crop = get_crop(params.cs, params.area);
    if (!DP_rect_valid(crop) || !params.frame_indexes
        || params.frame_index_count <= 0) {
        DP_error_set("Invalid arguments");
        return DP_SAVE_RESULT_BAD_ARGUMENTS;
    }

    int input_width = DP_rect_width(crop);
    int input_height = DP_rect_height(crop);
    int output_width =
        get_format_dimension(DP_SAVE_VIDEO_FORMAT_GIF,
                             params.width > 0 ? params.width : input_width);
    int output_height =
        get_format_dimension(DP_SAVE_VIDEO_FORMAT_GIF,
                             params.height > 0 ? params.height : input_height);
    if (!check_format_dimensions(output_width, output_height,
                                 DP_SAVE_VIDEO_FORMAT_GIF)) {
        return DP_SAVE_RESULT_BAD_DIMENSIONS;
    }

    // Generate GIF palette into an in-memory buffer.
    void **buffer_ptr;
    size_t *size_ptr;
    DP_Output *palette_output = DP_mem_output_new(
        DP_SAVE_VIDEO_GIF_PALETTE_BYTES, true, &buffer_ptr, &size_ptr);
    if (!palette_output) {
        return DP_SAVE_RESULT_INTERNAL_ERROR;
    }

    DP_SaveResult result =
        DP_save_animation_video((DP_SaveAnimationVideoParams){
            params.cs, params.area, DP_SAVE_VIDEO_DESTINATION_OUTPUT,
            palette_output, NULL, 0, params.flags, DP_SAVE_VIDEO_FORMAT_PALETTE,
            params.width, params.height, params.frame_indexes,
            params.frame_index_count, params.framerate, 1,
            on_gif_progress_palette, &params});

    if (result == DP_SAVE_RESULT_SUCCESS) {
        // Render out the actual GIF based on the generated palette.
        result = DP_save_animation_video((DP_SaveAnimationVideoParams){
            params.cs, params.area, params.destination, params.path_or_output,
            *buffer_ptr, *size_ptr, params.flags, DP_SAVE_VIDEO_FORMAT_GIF,
            params.width, params.height, params.frame_indexes,
            params.frame_index_count, params.framerate, 1, on_gif_progress_gif,
            &params});
    }

    DP_output_free(palette_output);
    return result;
}
