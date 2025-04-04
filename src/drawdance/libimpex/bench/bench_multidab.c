// SPDX-License-Identifier: GPL-3.0-or-later
#include "limits.h"
#include <dpcommon/common.h>
#include <dpcommon/conversions.h>
#include <dpcommon/cpu.h>
#include <dpcommon/output.h>
#include <dpcommon/perf.h>
#include <dpengine/canvas_state.h>
#include <dpengine/draw_context.h>
#include <dpengine/image.h>
#include <dpengine/layer_content.h>
#include <dpengine/layer_list.h>
#include <dpengine/layer_props.h>
#include <dpengine/layer_props_list.h>
#include <dpengine/tile.h>
#include <dpimpex/image_impex.h>
#include <dpmsg/blend_mode.h>
#include <dpmsg/message.h>
#include <stdio.h>


#define LAYER_ID      0x100
#define CANVAS_WIDTH  1000
#define CANVAS_HEIGHT 1000


struct PixelArgs {
    DP_BlendMode blend_mode;
    uint8_t size;
    uint8_t opacity;
};

struct ClassicArgs {
    DP_BlendMode blend_mode;
    uint16_t size;
    uint8_t opacity;
    uint8_t hardness;
};

struct MyPaintArgs {
    uint16_t size;
    uint8_t opacity;
    uint8_t hardness;
    uint8_t angle;
    uint8_t aspect_ratio;
    uint8_t lock_alpha;
    uint8_t colorize;
    uint8_t posterize;
    uint8_t posterize_num;
};

struct Args {
    DP_MessageType type;
    const char *save_path;
    int total_dabs;
    int dabs_per_message;
    uint8_t color_alpha;
    union {
        struct PixelArgs pixel;
        struct ClassicArgs classic;
        struct MyPaintArgs mypaint;
    };
};

struct DabParams {
    const struct Args *args;
    int dab_count;
};

static bool parse_llong_arg(const char *s, long long min_inclusive,
                            long long max_inclusive, long long *out_value)
{
    char *end;
    long long value = strtoll(s, &end, 10);
    if (*end != '\0') {
        DP_warn("Can't parse '%s'", s);
        return false;
    }
    else if (value < min_inclusive || value > max_inclusive) {
        DP_warn("%lld out of bounds, min %lld, max %lld", value, min_inclusive,
                max_inclusive);
        return false;
    }
    else {
        *out_value = value;
        return true;
    }
}

static bool parse_int_arg(const char *s, int min_inclusive, int max_inclusive,
                          int *out_value)
{
    long long value;
    if (parse_llong_arg(s, min_inclusive, max_inclusive, &value)) {
        *out_value = DP_llong_to_int(value);
        return true;
    }
    else {
        return false;
    }
}

static bool parse_uint8_arg(const char *s, uint8_t min_inclusive,
                            uint8_t max_inclusive, uint8_t *out_value)
{
    long long value;
    if (parse_llong_arg(s, min_inclusive, max_inclusive, &value)) {
        *out_value = DP_llong_to_uint8(value);
        return true;
    }
    else {
        return false;
    }
}

static bool parse_uint16_arg(const char *s, uint16_t min_inclusive,
                             uint16_t max_inclusive, uint16_t *out_value)
{
    long long value;
    if (parse_llong_arg(s, min_inclusive, max_inclusive, &value)) {
        *out_value = DP_llong_to_uint16(value);
        return true;
    }
    else {
        return false;
    }
}

static bool parse_blend_mode_arg(const char *s, DP_BlendMode *out_value)
{
    DP_BlendMode value = DP_blend_mode_by_dptxt_name(s, DP_BLEND_MODE_COUNT);
    if (value == DP_BLEND_MODE_COUNT) {
        DP_warn("Unknown blend mode '%s'", s);
        return false;
    }
    else if (!DP_blend_mode_valid_for_brush((int)value)) {
        DP_warn("Blend mode '%s' not valid for brush", s);
        return false;
    }
    else {
        *out_value = value;
        return true;
    }
}

static bool parse_common_arguments(DP_MessageType type, int required_argc,
                                   int argc, char **argv, struct Args *out_args)
{
    if (argc == required_argc || argc == required_argc + 1) {
        out_args->type = type;
        out_args->save_path = argc > required_argc ? argv[required_argc] : NULL;
        return true;
    }
    else {
        DP_warn("Wrong number of arguments");
        return false;
    }
}

static bool parse_pixel_arguments(DP_MessageType type, int argc, char **argv,
                                  struct Args *out_args)
{
    return parse_common_arguments(type, 8, argc, argv, out_args)
        && parse_int_arg(argv[2], 0, INT_MAX, &out_args->total_dabs)
        && parse_int_arg(argv[3], DP_MSG_DRAW_DABS_PIXEL_DABS_MIN_COUNT,
                         DP_min_int(DP_MSG_DRAW_DABS_PIXEL_DABS_MAX_COUNT,
                                    out_args->total_dabs),
                         &out_args->dabs_per_message)
        && parse_uint8_arg(argv[4], 0, UINT8_MAX, &out_args->color_alpha)
        && parse_blend_mode_arg(argv[5], &out_args->pixel.blend_mode)
        && parse_uint8_arg(argv[6], 0, UINT8_MAX, &out_args->pixel.size)
        && parse_uint8_arg(argv[7], 0, UINT8_MAX, &out_args->pixel.opacity);
}

static bool parse_classic_arguments(int argc, char **argv,
                                    struct Args *out_args)
{
    return parse_common_arguments(DP_MSG_DRAW_DABS_CLASSIC, 9, argc, argv,
                                  out_args)
        && parse_int_arg(argv[2], 0, INT_MAX, &out_args->total_dabs)
        && parse_int_arg(argv[3], DP_MSG_DRAW_DABS_CLASSIC_DABS_MIN_COUNT,
                         DP_min_int(DP_MSG_DRAW_DABS_CLASSIC_DABS_MAX_COUNT,
                                    out_args->total_dabs),
                         &out_args->dabs_per_message)
        && parse_uint8_arg(argv[4], 0, UINT8_MAX, &out_args->color_alpha)
        && parse_blend_mode_arg(argv[5], &out_args->classic.blend_mode)
        && parse_uint16_arg(argv[6], 0, UINT16_MAX, &out_args->classic.size)
        && parse_uint8_arg(argv[7], 0, UINT8_MAX, &out_args->classic.opacity)
        && parse_uint8_arg(argv[8], 0, UINT8_MAX, &out_args->classic.hardness);
}

static bool parse_mypaint_arguments(int argc, char **argv,
                                    struct Args *out_args)
{
    return parse_common_arguments(DP_MSG_DRAW_DABS_MYPAINT, 14, argc, argv,
                                  out_args)
        && parse_int_arg(argv[2], 0, INT_MAX, &out_args->total_dabs)
        && parse_int_arg(argv[3], DP_MSG_DRAW_DABS_MYPAINT_DABS_MIN_COUNT,
                         DP_min_int(DP_MSG_DRAW_DABS_MYPAINT_DABS_MAX_COUNT,
                                    out_args->total_dabs),
                         &out_args->dabs_per_message)
        && parse_uint8_arg(argv[4], 0, UINT8_MAX, &out_args->color_alpha)
        && parse_uint16_arg(argv[5], 0, UINT16_MAX, &out_args->mypaint.size)
        && parse_uint8_arg(argv[6], 0, UINT8_MAX, &out_args->mypaint.opacity)
        && parse_uint8_arg(argv[7], 0, UINT8_MAX, &out_args->mypaint.hardness)
        && parse_uint8_arg(argv[8], 0, UINT8_MAX, &out_args->mypaint.angle)
        && parse_uint8_arg(argv[9], 0, UINT8_MAX,
                           &out_args->mypaint.aspect_ratio)
        && parse_uint8_arg(argv[10], 0, UINT8_MAX,
                           &out_args->mypaint.lock_alpha)
        && parse_uint8_arg(argv[11], 0, UINT8_MAX, &out_args->mypaint.colorize)
        && parse_uint8_arg(argv[12], 0, UINT8_MAX, &out_args->mypaint.posterize)
        && parse_uint8_arg(argv[13], 0, UINT8_MAX,
                           &out_args->mypaint.posterize_num);
}

static bool parse_arguments(int argc, char **argv, struct Args *out_args)
{
    if (argc < 2) {
        DP_warn("Not enough arguments");
        return false;
    }

    const char *type = argv[1];
    if (DP_str_equal(type, "pixelround")) {
        return parse_pixel_arguments(DP_MSG_DRAW_DABS_PIXEL, argc, argv,
                                     out_args);
    }
    else if (DP_str_equal(type, "pixelsquare")) {
        return parse_pixel_arguments(DP_MSG_DRAW_DABS_PIXEL_SQUARE, argc, argv,
                                     out_args);
    }
    else if (DP_str_equal(type, "classic")) {
        return parse_classic_arguments(argc, argv, out_args);
    }
    else if (DP_str_equal(type, "mypaint")) {
        return parse_mypaint_arguments(argc, argv, out_args);
    }
    else if (DP_str_equal(type, "blendmodes")) {
        if (argc == 2) {
            out_args->type = DP_MSG_TYPE_COUNT;
            return true;
        }
        else {
            DP_warn("Wrong number of arguments");
            return false;
        }
    }
    else {
        DP_warn("Invalid type '%s'", type);
        return false;
    }
}


static void set_pixel_dabs(int count, DP_PixelDab *pds, void *user)
{
    const struct Args *args = user;
    for (int i = 0; i < count; ++i) {
        DP_pixel_dab_init(pds, i, 0, 0, args->pixel.size, args->pixel.opacity);
    }
}

static void set_classic_dabs(int count, DP_ClassicDab *cds, void *user)
{
    const struct Args *args = user;
    for (int i = 0; i < count; ++i) {
        DP_classic_dab_init(cds, i, 0, 0, args->classic.size,
                            args->classic.hardness, args->classic.opacity);
    }
}

static void set_mypaint_dabs(int count, DP_MyPaintDab *mpds, void *user)
{
    const struct Args *args = user;
    for (int i = 0; i < count; ++i) {
        DP_mypaint_dab_init(mpds, i, 0, 0, args->mypaint.size,
                            args->mypaint.hardness, args->mypaint.opacity,
                            args->mypaint.angle, args->mypaint.aspect_ratio);
    }
}

static DP_Message **generate_messages(const struct Args *args, int *out_count)
{
    int total_dabs = args->total_dabs;
    int dabs_per_message = args->dabs_per_message;
    int count = total_dabs / dabs_per_message;
    if (total_dabs % dabs_per_message != 0) {
        ++count;
    }

    DP_Message **msgs = DP_malloc(sizeof(*msgs) * DP_int_to_size(count));
    int dabs_done = 0;
    for (int i = 0; i < count; ++i) {
        int dab_count = DP_min_int(dabs_per_message, total_dabs - dabs_done);
        DP_UPixel8 pixel = {.b = DP_int_to_uint8(rand() % UINT8_MAX),
                            .g = DP_int_to_uint8(rand() % UINT8_MAX),
                            .r = DP_int_to_uint8(rand() % UINT8_MAX),
                            .a = args->color_alpha};

        switch (args->type) {
        case DP_MSG_DRAW_DABS_PIXEL:
            msgs[i] = DP_msg_draw_dabs_pixel_new(
                1, LAYER_ID, CANVAS_WIDTH / 2, CANVAS_HEIGHT / 2, pixel.color,
                (uint8_t)args->pixel.blend_mode, set_pixel_dabs, dab_count,
                (void *)args);
            break;
        case DP_MSG_DRAW_DABS_PIXEL_SQUARE:
            msgs[i] = DP_msg_draw_dabs_pixel_square_new(
                1, LAYER_ID, CANVAS_WIDTH / 2, CANVAS_HEIGHT / 2, pixel.color,
                (uint8_t)args->pixel.blend_mode, set_pixel_dabs, dab_count,
                (void *)args);
            break;
        case DP_MSG_DRAW_DABS_CLASSIC:
            msgs[i] = DP_msg_draw_dabs_classic_new(
                1, LAYER_ID, DP_double_to_int32(CANVAS_WIDTH / 2.0 * 4.0),
                DP_double_to_int32(CANVAS_HEIGHT / 2.0 * 4.0), pixel.color,
                (uint8_t)args->pixel.blend_mode, set_classic_dabs, dab_count,
                (void *)args);
            break;
        case DP_MSG_DRAW_DABS_MYPAINT:
            msgs[i] = DP_msg_draw_dabs_mypaint_new(
                1, LAYER_ID, DP_double_to_int32(CANVAS_WIDTH / 2.0 * 4.0),
                DP_double_to_int32(CANVAS_HEIGHT / 2.0 * 4.0), pixel.color,
                args->mypaint.lock_alpha, args->mypaint.colorize,
                args->mypaint.posterize, args->mypaint.posterize_num,
                set_mypaint_dabs, dab_count, (void *)args);
            break;
        default:
            DP_UNREACHABLE();
        }

        dabs_done += dab_count;
    }

    *out_count = count;
    return msgs;
}

static void free_messages(int count, DP_Message **msgs)
{
    for (int i = 0; i < count; ++i) {
        DP_message_decref(msgs[i]);
    }
    DP_free(msgs);
}


static DP_CanvasState *init_canvas_state(DP_DrawContext *dc)
{
    DP_TransientCanvasState *tcs = DP_transient_canvas_state_new_init();
    DP_transient_canvas_state_width_set(tcs, CANVAS_WIDTH);
    DP_transient_canvas_state_height_set(tcs, CANVAS_HEIGHT);

    DP_TransientLayerList *tll =
        DP_transient_canvas_state_transient_layers(tcs, 1);
    DP_TransientLayerPropsList *tlpl =
        DP_transient_canvas_state_transient_layer_props(tcs, 1);

    DP_Tile *t = DP_tile_new_from_bgra(0, 0xffffffff);
    DP_TransientLayerContent *tlc = DP_transient_layer_content_new_init(
        DP_transient_canvas_state_width(tcs),
        DP_transient_canvas_state_height(tcs), t);
    DP_tile_decref(t);

    DP_TransientLayerProps *tlp =
        DP_transient_layer_props_new_init(LAYER_ID, false);
    const char *title = "Layer 1";
    DP_transient_layer_props_title_set(tlp, title, strlen(title));

    DP_transient_layer_list_insert_transient_content_noinc(tll, tlc, 0);
    DP_transient_layer_props_list_insert_transient_noinc(tlpl, tlp, 0);

    DP_transient_canvas_state_layer_routes_reindex(tcs, dc);
    return DP_transient_canvas_state_persist(tcs);
}

static bool save_image(DP_CanvasState *cs, const char *save_path)
{
    DP_Image *img = DP_canvas_state_to_flat_image(
        cs, DP_FLAT_IMAGE_RENDER_FLAGS, NULL, NULL);
    if (!img) {
        return false;
    }

    DP_Output *output = DP_file_output_new_from_path(save_path);
    if (!output) {
        DP_image_free(img);
        return false;
    }

    bool ok = DP_image_write_png(img, output);
    DP_output_free(output);
    DP_image_free(img);
    return ok;
}

static void bench(int count, DP_Message **msgs, const char *save_path)
{
    DP_DrawContext *dc = DP_draw_context_new();
    DP_CanvasState *cs = init_canvas_state(dc);
    unsigned long long start = DP_perf_time();
    DP_CanvasState *next_cs =
        DP_canvas_state_handle_multidab(cs, dc, NULL, count, msgs);
    unsigned long long end = DP_perf_time();

    if (save_path && !save_image(next_cs, save_path)) {
        DP_warn("Error saving to '%s': %s", save_path, DP_error());
    }

    DP_canvas_state_decref(next_cs);
    DP_canvas_state_decref(cs);
    DP_draw_context_free(dc);
    printf("%llu\n", end - start);
}

static void run(const struct Args *args)
{
    DP_cpu_support_init();
    srand(0);
    int count;
    DP_Message **msgs = generate_messages(args, &count);
    bench(count, msgs, args->save_path);
    free_messages(count, msgs);
}

static void dump_blend_modes(void)
{
    printf("# Generated by running `bench_multidab blendmodes`\n");
    printf("blend_modes = [\n");
    for (int i = 0; i < DP_BLEND_MODE_COUNT; ++i) {
        if (DP_blend_mode_valid_for_brush(i)) {
            printf("    {\"enum_name\": \"%s\", \"dptxt_name\": \"%s\"",
                   DP_blend_mode_enum_name(i), DP_blend_mode_dptxt_name(i));
            if (DP_blend_mode_secondary_alias(i)) {
                DP_BlendMode alpha_affecting, alpha_preserving;
                if (DP_blend_mode_alpha_preserve_pair(i, &alpha_affecting,
                                                      &alpha_preserving)) {
                    if ((int)alpha_affecting == i) {
                        printf(", \"alias_for\": \"%s\"",
                               DP_blend_mode_enum_name((int)alpha_preserving));
                    }
                    else if ((int)alpha_preserving == i) {
                        printf(", \"alias_for\": \"%s\"",
                               DP_blend_mode_enum_name((int)alpha_affecting));
                    }
                    else {
                        DP_warn("Blend mode %d is aliased to neither "
                                "alpha-affecting mode %d nor alpha-preserving "
                                "mode %d",
                                i, (int)alpha_affecting, (int)alpha_preserving);
                    }
                }
                else {
                    DP_warn("Blend mode %d purports to be aliased but has no "
                            "alpha preserve pair",
                            i);
                }
            }
            printf("},\n");
        }
    }
    printf("]\n");
}

int main(int argc, char **argv)
{
    struct Args args;
    if (parse_arguments(argc, argv, &args)) {
        if (args.type == DP_MSG_TYPE_COUNT) {
            dump_blend_modes();
        }
        else {
            run(&args);
        }
        return 0;
    }
    else {
        return 2;
    }
}
