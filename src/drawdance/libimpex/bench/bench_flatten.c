// SPDX-License-Identifier: GPL-3.0-or-later
#include <dpcommon/common.h>
#include <dpcommon/conversions.h>
#include <dpcommon/cpu.h>
#include <dpcommon/output.h>
#include <dpengine/canvas_state.h>
#include <dpengine/image.h>
#include <dpengine/layer_content.h>
#include <dpengine/layer_list.h>
#include <dpengine/layer_props.h>
#include <dpengine/layer_props_list.h>
#include <dpimpex/image_impex.h>
#include <dpmsg/blend_mode.h>
#include <math.h>
#include <rng-double.h>
#include <stdio.h>

static bool parse_args(int argc, char **argv, int *out_width, int *out_height,
                       int *out_iterations, long *out_seed, int *out_mode,
                       const char **out_path)
{
    int min_args = 6;
    if (argc != min_args && argc != min_args + 1) {
        fprintf(stderr, "Expected %d or %d arguments, but got %d\n",
                min_args - 1, min_args, argc - 1);
        return false;
    }

    int width = atoi(argv[1]);
    if (!DP_canvas_state_in_max_dimension_bound(width)) {
        fputs("Width out of bounds\n", stderr);
        return false;
    }

    int height = atoi(argv[2]);
    if (!DP_canvas_state_in_max_dimension_bound(height)) {
        fputs("Height out of bounds\n", stderr);
        return false;
    }

    if (!DP_canvas_state_in_max_pixels_bound(width, height)) {
        fputs("Dimensions out of bounds\n", stderr);
        return false;
    }

    int iterations = atoi(argv[3]);
    long seed = atol(argv[4]);

    DP_BlendMode mode = DP_blend_mode_by_ora_name(argv[5], DP_BLEND_MODE_COUNT);
    if (mode == DP_BLEND_MODE_COUNT) {
        fprintf(stderr, "Unknown blend mode '%s'\n", argv[5]);
        return false;
    }

    *out_width = width;
    *out_height = height;
    *out_iterations = iterations;
    *out_seed = seed;
    *out_mode = (int)mode;
    *out_path = argc < 7 ? NULL : argv[6];
    return true;
}

static double get_random(RngDouble *rng)
{
    return fabs(fmod(rng_double_next(rng), 1.0));
}

static DP_TransientLayerProps *
generate_layer_props(int layer_id, int blend_mode, uint16_t opacity)
{
    DP_TransientLayerProps *tlp =
        DP_transient_layer_props_new_init(layer_id, false);
    DP_transient_layer_props_blend_mode_set(tlp, blend_mode);
    DP_transient_layer_props_opacity_set(tlp, opacity);
    return tlp;
}

static DP_TransientLayerContent *generate_layer_content(RngDouble *rng,
                                                        int width, int height)
{
    DP_TransientLayerContent *tlc =
        DP_transient_layer_content_new_init(width, height, NULL);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            double a = get_random(rng);
            DP_Pixel15 pixel = {
                DP_channel_float_to_15(DP_double_to_float(get_random(rng) * a)),
                DP_channel_float_to_15(DP_double_to_float(get_random(rng) * a)),
                DP_channel_float_to_15(DP_double_to_float(get_random(rng) * a)),
                DP_channel_float_to_15(DP_double_to_float(a)),
            };
            DP_transient_layer_content_pixel_at_set(tlc, 0, x, y, pixel);
        }
    }

    return tlc;
}

static DP_CanvasState *generate_canvas(RngDouble *rng, int width, int height,
                                       int mode)
{
    DP_TransientCanvasState *tcs = DP_transient_canvas_state_new_init();
    DP_transient_canvas_state_width_set(tcs, width);
    DP_transient_canvas_state_height_set(tcs, height);

    DP_TransientLayerList *tll =
        DP_transient_canvas_state_transient_layers(tcs, 2);
    DP_transient_layer_list_set_transient_content_noinc(
        tll, generate_layer_content(rng, width, height), 0);
    DP_transient_layer_list_set_transient_content_noinc(
        tll, generate_layer_content(rng, width, height), 1);

    DP_TransientLayerPropsList *tlpl =
        DP_transient_canvas_state_transient_layer_props(tcs, 2);
    DP_transient_layer_props_list_set_transient_noinc(
        tlpl,
        generate_layer_props(
            1, DP_BLEND_MODE_NORMAL,
            DP_channel_float_to_15(DP_double_to_float(get_random(rng)))),
        0);
    DP_transient_layer_props_list_set_transient_noinc(
        tlpl,
        generate_layer_props(
            2, mode,
            DP_channel_float_to_15(DP_double_to_float(get_random(rng)))),
        1);

    return DP_transient_canvas_state_persist(tcs);
}

int main(int argc, char **argv)
{
    DP_cpu_support_init();
    DP_image_impex_init();

    int width, height, iterations, mode;
    long seed;
    const char *path;
    if (!parse_args(argc, argv, &width, &height, &iterations, &seed, &mode,
                    &path)) {
        fprintf(stderr,
                "Usage: %s WIDTH HEIGHT ITERATIONS SEED MODE [PATH_TO_PNG]\n",
                argc > 0 && argv[0] ? argv[0] : "bench_flatten");
        return 2;
    }

    RngDouble *rng = rng_double_new(seed);
    DP_CanvasState *cs = generate_canvas(rng, width, height, mode);
    rng_double_free(rng);

    for (int i = 0; i < iterations; ++i) {
        DP_TransientLayerContent *tlc =
            DP_canvas_state_to_flat_layer(cs, DP_FLAT_IMAGE_RENDER_FLAGS, NULL);
        if (tlc) {
            DP_transient_layer_content_decref(tlc);
        }
    }

    if (path) {
        DP_Output *out = DP_file_output_new_from_path(path);
        if (!out) {
            DP_canvas_state_decref(cs);
            fprintf(stderr, "%s\n", DP_error());
            return 1;
        }

        DP_Image *img = DP_canvas_state_to_flat_image(
            cs, DP_FLAT_IMAGE_RENDER_FLAGS, NULL, NULL);
        DP_canvas_state_decref(cs);
        if (!img) {
            fprintf(stderr, "%s\n", DP_error());
            DP_output_free_discard(out);
            return 1;
        }

        if (!DP_image_write_png(img, out)) {
            fprintf(stderr, "%s\n", DP_error());
            DP_output_free_discard(out);
            return 1;
        }

        if (!DP_output_free(out)) {
            fprintf(stderr, "%s\n", DP_error());
            return 1;
        }

        return 0;
    }
    else {
        DP_canvas_state_decref(cs);
        return 0;
    }
}
