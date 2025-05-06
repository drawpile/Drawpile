// SPDX-License-Identifier: GPL-3.0-or-later
#include "save_video.h"
#include "save.h"
#include <dpcommon/common.h>
#include <dpcommon/conversions.h>
#include <dpcommon/geom.h>
#include <dpcommon/output.h>
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
                              void *path_or_output)
{
    switch (destination) {
    case DP_SAVE_VIDEO_DESTINATION_PATH: {
        const char *path = path_or_output;
        return path && path[0] != '\0';
    }
    case DP_SAVE_VIDEO_DESTINATION_OUTPUT: {
        DP_Output *output = path_or_output;
        return output != NULL;
    }
    default:
        return false;
    }
}

static enum AVCodecID get_format_codec_id(int format)
{
    switch (format) {
    case DP_SAVE_VIDEO_FORMAT_MP4:
        return AV_CODEC_ID_H264;
    case DP_SAVE_VIDEO_FORMAT_WEBM:
        return AV_CODEC_ID_VP8;
    case DP_SAVE_VIDEO_FORMAT_WEBP:
        return AV_CODEC_ID_WEBP;
    case DP_SAVE_VIDEO_FORMAT_PALETTE:
        return AV_CODEC_ID_RAWVIDEO;
    case DP_SAVE_VIDEO_FORMAT_GIF:
        return AV_CODEC_ID_GIF;
    default:
        return AV_CODEC_ID_NONE;
    }
}

bool DP_save_video_format_supported(int format)
{
    enum AVCodecID codec_id = get_format_codec_id(format);
    return codec_id != AV_CODEC_ID_NONE && avcodec_find_encoder(codec_id);
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
    case DP_SAVE_VIDEO_FORMAT_MP4:
        return "mp4";
    case DP_SAVE_VIDEO_FORMAT_WEBM:
        return "webm";
    case DP_SAVE_VIDEO_FORMAT_WEBP:
        return "webp";
    case DP_SAVE_VIDEO_FORMAT_PALETTE:
        return "rawvideo";
    case DP_SAVE_VIDEO_FORMAT_GIF:
        return "gif";
    default:
        return NULL;
    }
}

static bool check_format_dimensions(int output_width, int output_height,
                                    int format)
{
    switch (format) {
    case DP_SAVE_VIDEO_FORMAT_MP4:
        return DP_save_check_width_height(output_width, output_height,
                                          2147483646);
    case DP_SAVE_VIDEO_FORMAT_WEBM:
    case DP_SAVE_VIDEO_FORMAT_GIF:
        return DP_save_check_width_height(output_width, output_height, 65536);
    case DP_SAVE_VIDEO_FORMAT_WEBP:
        return DP_save_check_width_height(output_width, output_height, 16384);
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
        return 1;
    default:
        return loops < 1 ? 1 : loops;
    }
}

static int get_format_dimension(int format, int target_dimension,
                                int input_dimension)
{
    int dimension = target_dimension > 0 ? target_dimension : input_dimension;
    switch (format) {
    case DP_SAVE_VIDEO_FORMAT_MP4:
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
    // 1 pixel difference may be due to get_format_dimensions padding for H264.
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
    default:
        return AV_PIX_FMT_YUV420P;
    }
}

static void set_option(void *obj, const char *name, const char *value)
{
    int result = av_opt_set(obj, name, value, AV_OPT_SEARCH_CHILDREN);
    if (result == 0) {
        DP_info("Set option %s to %s", name, value);
    }
    else {
        DP_warn("Error setting option %s to %s: %s", name, value,
                av_err2str(result));
    }
}

static void set_format_codec_params(int format, AVCodecContext *codec_context)
{
    switch (format) {
    case DP_SAVE_VIDEO_FORMAT_MP4:
    case DP_SAVE_VIDEO_FORMAT_PALETTE:
    case DP_SAVE_VIDEO_FORMAT_GIF:
        break;
    case DP_SAVE_VIDEO_FORMAT_WEBM:
        codec_context->bit_rate = 1 * 1024 * 1024;
        set_option(codec_context->priv_data, "crf", "15");
        break;
    case DP_SAVE_VIDEO_FORMAT_WEBP:
        set_option(codec_context->priv_data, "lossless", "1");
        set_option(codec_context->priv_data, "preset", "drawing");
        set_option(codec_context->priv_data, "quality", "100");
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
    case DP_SAVE_VIDEO_FORMAT_MP4:
    case DP_SAVE_VIDEO_FORMAT_WEBM:
    case DP_SAVE_VIDEO_FORMAT_PALETTE:
    case DP_SAVE_VIDEO_FORMAT_GIF:
        break;
    case DP_SAVE_VIDEO_FORMAT_WEBP:
        set_option(format_context->priv_data, "loop", "0");
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

static int64_t get_format_frame_duration(int format,
                                         AVCodecContext *codec_context,
                                         AVStream *stream)
{
    switch (format) {
    case DP_SAVE_VIDEO_FORMAT_WEBP:
    case DP_SAVE_VIDEO_FORMAT_PALETTE:
        return 1;
    default: {
        AVRational pts = av_div_q(codec_context->time_base, stream->time_base);
        return DP_double_to_int64(av_q2d(pts) + 0.5);
    }
    }
}

static unsigned int get_format_flat_image_flags(int format)
{
    switch (format) {
    case DP_SAVE_VIDEO_FORMAT_PALETTE:
    case DP_SAVE_VIDEO_FORMAT_GIF:
        return DP_FLAT_IMAGE_RENDER_FLAGS | DP_FLAT_IMAGE_ONE_BIT_ALPHA;
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

static bool report_progress(DP_SaveAnimationProgressFn progress_fn, void *user,
                            double progress)
{
    return !progress_fn || progress_fn(user, progress);
}

static bool report_frame_progress(DP_SaveAnimationProgressFn progress_fn,
                                  void *user, int frames_done, int frames_to_do)
{
    double part = DP_int_to_double(frames_done);
    double total = DP_int_to_double(frames_to_do);
    return report_progress(progress_fn, user, part / total * 0.97);
}

static DP_SaveResult handle_frame(AVCodecContext *codec_context,
                                  AVFormatContext *format_context,
                                  AVFrame *frame, AVPacket *packet)
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

        err = av_interleaved_write_frame(format_context, packet);
        if (err != 0) {
            DP_error_set("Error writing frame: %s", av_err2str(err));
            return DP_SAVE_RESULT_WRITE_ERROR;
        }
    }
}

static DP_SaveResult filter_frame(AVCodecContext *codec_context,
                                  AVFormatContext *format_context,
                                  AVFrame *frame, AVPacket *packet,
                                  AVFilterContext *buffersrc_context,
                                  AVFilterContext *buffersink_context,
                                  AVFrame *filtered_frame)
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
                                                filtered_frame, packet);
            av_frame_unref(filtered_frame);
            if (result != DP_SAVE_RESULT_SUCCESS) {
                return result;
            }
        }
    }
    else {
        DP_ASSERT(!buffersink_context);
        DP_ASSERT(!filtered_frame);
        return handle_frame(codec_context, format_context, frame, packet);
    }
}

DP_SaveResult DP_save_animation_video(DP_SaveVideoParams params)
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
    DP_ViewModeBuffer vmb;
    DP_view_mode_buffer_init(&vmb);
    DP_Image *img = NULL;

    DP_Rect crop;
    const char *format_name;
    enum AVCodecID codec_id;
    bool params_ok =
        params.cs
        && is_destination_ok(params.destination, params.path_or_output)
        && params.width > 0 && params.height > 0
        && DP_rect_valid(crop = get_crop(params.cs, params.area))
        && (format_name = get_format_name(params.format))
        && (codec_id = get_format_codec_id(params.format)) != AV_CODEC_ID_NONE;
    if (!params_ok) {
        DP_error_set("Invalid arguments");
        result = DP_SAVE_RESULT_BAD_ARGUMENTS;
        goto cleanup;
    }

    int input_width = DP_rect_width(crop);
    int input_height = DP_rect_height(crop);
    int output_width =
        get_format_dimension(params.format, params.width, input_width);
    int output_height =
        get_format_dimension(params.format, params.height, input_height);
    if (!check_format_dimensions(output_width, output_height, params.format)) {
        result = DP_SAVE_RESULT_BAD_DIMENSIONS;
        goto cleanup;
    }

    if (params.palette_data
        && params.palette_size != DP_SAVE_VIDEO_GIF_PALETTE_BYTES) {
        DP_error_set("Incorrect palette size, got %zu, should be %zu",
                     params.palette_size, DP_SAVE_VIDEO_GIF_PALETTE_BYTES);
        result = DP_SAVE_RESULT_BAD_ARGUMENTS;
        goto cleanup;
    }

    int start = params.start < 0 ? 0 : params.start;
    int frame_count = DP_canvas_state_frame_count(params.cs);
    int end_inclusive =
        params.end_inclusive < 0 || params.end_inclusive > frame_count - 1
            ? frame_count - 1
            : params.end_inclusive;
    if (start > end_inclusive) {
        DP_error_set("Frame range is empty");
        result = DP_SAVE_RESULT_BAD_ARGUMENTS;
        goto cleanup;
    }

    int framerate = params.framerate <= 0 ? DP_canvas_state_framerate(params.cs)
                                          : params.framerate;
    if (framerate <= 0) {
        framerate = 24;
    }

    int loops = get_format_loops(params.format, params.loops);

    const AVOutputFormat *output_format =
        av_guess_format(format_name, NULL, NULL);
    if (!output_format) {
        DP_error_set("Failed to guess format for '%s'", format_name);
        result = DP_SAVE_RESULT_UNKNOWN_FORMAT;
        goto cleanup;
    }

    const AVCodec *codec = avcodec_find_encoder(codec_id);
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
    codec_context->framerate = av_make_q(framerate, 1);
    codec_context->time_base = av_make_q(1, framerate);
    set_format_codec_params(params.format, codec_context);

    int err = avformat_alloc_output_context2(
        &format_context, REMOVE_CONST(output_format), NULL,
        params.destination == DP_SAVE_VIDEO_DESTINATION_PATH
            ? params.path_or_output
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

        err =
            avfilter_graph_create_filter(&buffersink_context, buffersink_filter,
                                         "out", NULL, NULL, graph_context);
        if (err < 0) {
            DP_error_set("Error creating filter buffer sink: %s",
                         av_err2str(err));
            result = DP_SAVE_RESULT_INTERNAL_ERROR;
            goto cleanup;
        }

        const enum AVPixelFormat *pix_fmts =
            get_format_filter_pix_fmts(params.format);
        // av_opt_set_int_list is a really wonky macro under the hood, relying
        // on implicit conversions that trigger various warnings in that regard.
#if defined(__GNUC__)
#    pragma GCC diagnostic push
#    pragma GCC diagnostic ignored "-Wconversion"
#    pragma GCC diagnostic ignored "-Wsign-conversion"
#elif defined(_MSC_VER)
#    pragma warning(push)
#    pragma warning(disable : 4245)
#endif
        err = av_opt_set_int_list(buffersink_context, "pix_fmts", pix_fmts,
                                  AV_PIX_FMT_NONE, AV_OPT_SEARCH_CHILDREN);
#if defined(__GNUC__)
#    pragma GCC diagnostic pop
#elif defined(_MSC_VER)
#    pragma warning(pop)
#endif
        if (err < 0) {
            DP_error_set("Error setting buffersink pixel format: %s",
                         av_err2str(err));
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
        output = DP_file_output_new_from_path(params.path_or_output);
        if (!output) {
            result = DP_SAVE_RESULT_OPEN_ERROR;
            goto cleanup;
        }
    }
    else {
        output = params.path_or_output;
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

    sws_context = sws_getContext(input_width, input_height, AV_PIX_FMT_BGRA,
                                 output_width, output_height, frame->format,
                                 get_scaling_flags(params.flags, input_width,
                                                   input_height, output_width,
                                                   output_height),
                                 NULL, NULL, NULL);
    if (!sws_context) {
        DP_error_set("Failed to allocate scaling context");
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

    int frames_to_do = (end_inclusive - start + 1) * loops;
    int frames_done = 0;
    int64_t duration =
        get_format_frame_duration(params.format, codec_context, stream);
    int instances = 0;
    frame->pts = 0;
    unsigned int flat_image_flags = get_format_flat_image_flags(params.format);
    for (int loop_index = 0; loop_index < loops; ++loop_index) {
        int frame_index = start;
        while (frame_index <= end_inclusive) {
            err = av_frame_make_writable(frame);
            if (err != 0) {
                DP_error_set("Error making frame writeable: %s",
                             av_err2str(err));
                result = DP_SAVE_RESULT_INTERNAL_ERROR;
                goto cleanup;
            }

            instances = 1;
            while (frame_index < end_inclusive
                   && DP_canvas_state_same_frame(params.cs, frame_index,
                                                 frame_index + 1)) {
                ++frame_index;
                ++instances;
            }

            DP_ViewModeFilter vmf = DP_view_mode_filter_make_frame_render(
                &vmb, params.cs, frame_index - instances + 1);
            if (!DP_canvas_state_into_flat_image(params.cs, flat_image_flags,
                                                 &crop, &vmf, &img)) {
                result = DP_SAVE_RESULT_INTERNAL_ERROR;
                goto cleanup;
            }
            const uint8_t *data = (const uint8_t *)DP_image_pixels(img);
            const int stride = input_width * 4;
            sws_scale(sws_context, &data, &stride, 0, input_height, frame->data,
                      frame->linesize);

            result = filter_frame(codec_context, format_context, frame, packet,
                                  buffersrc_context, buffersink_context,
                                  filtered_frame);
            if (result != DP_SAVE_RESULT_SUCCESS) {
                goto cleanup;
            }

            frame->pts += duration * instances;
            frames_done += instances;
            ++frame_index;
            if (!report_frame_progress(params.progress_fn, params.user,
                                       frames_done, frames_to_do)) {
                result = DP_SAVE_RESULT_CANCEL;
                goto cleanup;
            }
        }
    }

    DP_image_free(img);
    img = NULL;

    if (instances > 1) {
        frame->pts -= duration;
        result =
            filter_frame(codec_context, format_context, frame, packet,
                         buffersrc_context, buffersink_context, filtered_frame);
        if (result != DP_SAVE_RESULT_SUCCESS) {
            goto cleanup;
        }
    }

    av_frame_free(&frame);

    if (!report_progress(params.progress_fn, params.user, 0.98)) {
        result = DP_SAVE_RESULT_CANCEL;
        goto cleanup;
    }

    result =
        filter_frame(codec_context, format_context, NULL, packet,
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
    DP_image_free(img);
    DP_view_mode_buffer_dispose(&vmb);
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


struct DP_GifProgressParams {
    bool palette;
    DP_SaveAnimationProgressFn progress_fn;
    void *user;
};

static bool on_gif_progress_palette(void *user, double progress)
{
    DP_SaveGifParams *params = user;
    DP_SaveAnimationProgressFn progress_fn = params->progress_fn;
    return progress_fn ? progress_fn(params->user, progress * 0.49) : true;
}

static bool on_gif_progress_gif(void *user, double progress)
{
    DP_SaveGifParams *params = user;
    DP_SaveAnimationProgressFn progress_fn = params->progress_fn;
    return progress_fn ? progress_fn(params->user, progress * 0.5 + 0.5) : true;
}

DP_SaveResult DP_save_animation_video_gif(DP_SaveGifParams params)
{
    DP_Rect crop = get_crop(params.cs, params.area);
    if (!DP_rect_valid(crop)) {
        DP_error_set("Invalid arguments");
        return DP_SAVE_RESULT_BAD_ARGUMENTS;
    }

    int input_width = DP_rect_width(crop);
    int input_height = DP_rect_height(crop);
    int output_width = get_format_dimension(DP_SAVE_VIDEO_FORMAT_GIF,
                                            params.width, input_width);
    int output_height = get_format_dimension(DP_SAVE_VIDEO_FORMAT_GIF,
                                             params.height, input_height);
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

    DP_SaveResult result = DP_save_animation_video((DP_SaveVideoParams){
        params.cs, params.area, DP_SAVE_VIDEO_DESTINATION_OUTPUT,
        palette_output, NULL, 0, params.flags, DP_SAVE_VIDEO_FORMAT_PALETTE,
        params.width, params.height, params.start, params.end_inclusive,
        params.framerate, 1, on_gif_progress_palette, &params});

    if (result == DP_SAVE_RESULT_SUCCESS) {
        // Render out the actual GIF based on the generated palette.
        result = DP_save_animation_video((DP_SaveVideoParams){
            params.cs, params.area, params.destination, params.path_or_output,
            *buffer_ptr, *size_ptr, params.flags, DP_SAVE_VIDEO_FORMAT_GIF,
            params.width, params.height, params.start, params.end_inclusive,
            params.framerate, 1, on_gif_progress_gif, &params});
    }

    DP_output_free(palette_output);
    return result;
}
