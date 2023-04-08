/*
 * Copyright (c) 2022 askmeaboutloom
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#include "canvas_renderer.h"
#include "gl.h"
#include <dpcommon/common.h>
#include <dpcommon/conversions.h>
#include <dpcommon/geom.h>
#include <dpengine/canvas_diff.h>
#include <dpengine/layer_content.h>
#include <dpengine/pixels.h>
#include <dpengine/tile.h>
#include <gles2_inc.h>
#include <math.h>


struct DP_CanvasRenderer {
    int width, height;
    bool recalculate_vertices;
    unsigned int program;
    unsigned int buffers[2];
    struct {
        float vertices[8];
        float uvs[8];
    } attributes;
    struct {
        int view;
        int sampler;
    } uniforms;
    struct {
        unsigned int id;
        int width, height;
    } texture;
    struct {
        double x, y;
        double scale;
        double rotation_in_radians;
    } transform;
    DP_Pixel8 pixels_buffer[DP_TILE_LENGTH];
};


static void init_texture(DP_CanvasRenderer *cr)
{
    DP_GL_CLEAR_ERROR();
    DP_GL(glActiveTexture, GL_TEXTURE0);
    DP_GL(glGenTextures, 1, &cr->texture.id);
    DP_GL(glBindTexture, GL_TEXTURE_2D, cr->texture.id);
    DP_GL(glTexParameteri, GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    DP_GL(glTexParameteri, GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    DP_GL(glTexParameteri, GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    DP_GL(glTexParameteri, GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    DP_GL(glBindTexture, GL_TEXTURE_2D, 0);
}

static unsigned int init_program(void)
{
    static const char *vert =
        "#version 100\n"
        "uniform vec2 u_view;\n"
        "attribute mediump vec2 v_pos;\n"
        "attribute mediump vec2 v_uv;\n"
        "varying vec2 f_uv;\n"
        "void main()\n"
        "{\n"
        "    gl_Position = vec4(2.0 * v_pos.x / u_view.x,\n"
        "                       -2.0 * v_pos.y / u_view.y,\n"
        "                       0.0, 1.0);\n"
        "    f_uv = v_uv;\n"
        "}\n";
    static const char *frag =
        "#version 100\n"
        "precision mediump float;\n"
        "uniform sampler2D u_sampler;\n"
        "varying vec2 f_uv;\n"
        "void main()\n"
        "{\n"
        "    gl_FragColor = texture2D(u_sampler, f_uv).bgra;\n"
        "}\n";
    return DP_gl_program_new(vert, frag);
}

DP_CanvasRenderer *DP_canvas_renderer_new(void)
{
    DP_CanvasRenderer *cr = DP_malloc(sizeof(*cr));
    (*cr) = (DP_CanvasRenderer){0,      0,         true,
                                0,      {0, 0},    {{0}, {0}},
                                {0, 0}, {0, 0, 0}, {0.0, 0.0, 1.0, 0.0},
                                {{0}}};
    cr->program = init_program();
    DP_GL(glGenBuffers, DP_ARRAY_LENGTH(cr->buffers), cr->buffers);
    cr->uniforms.view = glGetUniformLocation(cr->program, "u_view");
    DP_GL_CLEAR_ERROR();
    cr->uniforms.sampler = glGetUniformLocation(cr->program, "u_sampler");
    DP_GL_CLEAR_ERROR();
    init_texture(cr);
    return cr;
}

void DP_canvas_renderer_free(DP_CanvasRenderer *cr)
{
    if (cr) {
        DP_GL(glDeleteBuffers, DP_ARRAY_LENGTH(cr->buffers), cr->buffers);
        DP_GL(glDeleteProgram, cr->program);
        DP_GL(glDeleteTextures, 1, &cr->texture.id);
        DP_free(cr);
    }
}


void DP_canvas_renderer_transform(DP_CanvasRenderer *cr, double *out_x,
                                  double *out_y, double *out_scale,
                                  double *out_rotation_in_radians)
{
    DP_ASSERT(cr);
    if (out_x) {
        *out_x = cr->transform.x;
    }
    if (out_y) {
        *out_y = cr->transform.y;
    }
    if (out_scale) {
        *out_scale = cr->transform.scale;
    }
    if (out_rotation_in_radians) {
        *out_rotation_in_radians = cr->transform.rotation_in_radians;
    }
}

void DP_canvas_renderer_transform_set(DP_CanvasRenderer *cr, double x, double y,
                                      double scale, double rotation_in_radians)
{
    DP_ASSERT(cr);
    bool dirty = false;
    if (x != cr->transform.x) {
        cr->transform.x = x;
        dirty = true;
    }
    if (y != cr->transform.y) {
        cr->transform.y = y;
        dirty = true;
    }
    if (scale != cr->transform.scale) {
        cr->transform.scale = scale;
        dirty = true;
    }
    // TODO: modulo?
    if (rotation_in_radians != cr->transform.rotation_in_radians) {
        cr->transform.rotation_in_radians = rotation_in_radians;
        dirty = true;
    }
    if (dirty) {
        cr->recalculate_vertices = true;
    }
}


static int next_power_of_two(int x)
{
    return DP_double_to_int(
        pow(2.0, ceil(log(DP_int_to_double(x)) / log(2.0))));
}

static bool resize_texture(DP_CanvasRenderer *cr, int layer_width,
                           int layer_height)
{
    DP_ASSERT(cr->texture.width % DP_TILE_SIZE == 0);
    DP_ASSERT(cr->texture.height % DP_TILE_SIZE == 0);
    // It's enough to compare against the raw layer dimensions here,
    // since the texture dimensions are always tile-extended. So if
    // the raw dimensions fit, the tile-extended size will too.
    if (cr->texture.width < layer_width || cr->texture.height < layer_height) {
        int tiles_width = DP_tile_size_round_up(layer_width) * DP_TILE_SIZE;
        int tiles_height = DP_tile_size_round_up(layer_height) * DP_TILE_SIZE;
        int texture_width = next_power_of_two(tiles_width);
        int texture_height = next_power_of_two(tiles_height);
        cr->texture.width = texture_width;
        cr->texture.height = texture_height;
        DP_GL(glTexImage2D, GL_TEXTURE_2D, 0, GL_RGBA, texture_width,
              texture_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        return true;
    }
    else {
        return false;
    }
}

static void resize_attributes(DP_CanvasRenderer *cr, int layer_width,
                              int layer_height)
{
    cr->recalculate_vertices = true;
    float w = DP_int_to_float(layer_width);
    float h = DP_int_to_float(layer_height);
    float u = w / DP_int_to_float(cr->texture.width);
    float v = h / DP_int_to_float(cr->texture.height);
    float *uvs = cr->attributes.uvs;
    uvs[1] = v;
    uvs[4] = u;
    uvs[5] = v;
    uvs[6] = u;
}

static bool resize(DP_CanvasRenderer *cr, int layer_width, int layer_height)
{
    if (cr->width != layer_width || cr->height != layer_height) {
        bool texture_resized = resize_texture(cr, layer_width, layer_height);
        resize_attributes(cr, layer_width, layer_height);
        cr->width = layer_width;
        cr->height = layer_height;
        return texture_resized;
    }
    else {
        return false;
    }
}

static const DP_Pixel8 *get_tile_pixels(DP_CanvasRenderer *cr, DP_Tile *tile)
{
    static const DP_Pixel8 BLANK_PIXELS[DP_TILE_LENGTH];
    if (tile) {
        DP_Pixel8 *pixels = cr->pixels_buffer;
        DP_pixels15_to_8(pixels, DP_tile_pixels(tile), DP_TILE_LENGTH);
        return pixels;
    }
    else {
        return BLANK_PIXELS;
    }
}

static void write_tile_to_texture(DP_CanvasRenderer *cr, DP_LayerContent *lc,
                                  int tile_x, int tile_y)
{
    DP_Tile *tile = DP_layer_content_tile_at_noinc(lc, tile_x, tile_y);
    DP_GL(glTexSubImage2D, GL_TEXTURE_2D, 0, tile_x * DP_TILE_SIZE,
          tile_y * DP_TILE_SIZE, DP_TILE_SIZE, DP_TILE_SIZE, GL_RGBA,
          GL_UNSIGNED_BYTE, get_tile_pixels(cr, tile));
}

static void write_diff_tile_to_texture(void *user, int tile_x, int tile_y)
{
    DP_CanvasRenderer *cr = ((void **)user)[0];
    DP_LayerContent *lc = ((void **)user)[1];
    write_tile_to_texture(cr, lc, tile_x, tile_y);
}

static void write_all_tiles_to_texture(DP_CanvasRenderer *cr,
                                       DP_LayerContent *lc, int layer_width,
                                       int layer_height)
{
    DP_TileCounts counts = DP_tile_counts_round(layer_width, layer_height);
    for (int tile_y = 0; tile_y < counts.y; ++tile_y) {
        for (int tile_x = 0; tile_x < counts.x; ++tile_x) {
            write_tile_to_texture(cr, lc, tile_x, tile_y);
        }
    }
}

static DP_Transform calculate_transform(double x, double y, double scale,
                                        double rotation_in_radians)
{
    DP_Transform tf = DP_transform_identity();
    tf = DP_transform_rotate(tf, rotation_in_radians);
    tf = DP_transform_scale(tf, scale, scale);
    tf = DP_transform_translate(tf, x * scale, y * scale);
    return tf;
}

static void calculate_vertices(DP_CanvasRenderer *cr)
{
    double w = DP_int_to_float(cr->width);
    double h = DP_int_to_float(cr->height);
    double x2 = w * 0.5f, y2 = h * 0.5f;
    double x1 = -x2, y1 = -y2;

    DP_Transform tf = calculate_transform(cr->transform.x, cr->transform.y,
                                          cr->transform.scale,
                                          cr->transform.rotation_in_radians);
    DP_Vec2 tl = DP_transform_xy(tf, x1, y1);
    DP_Vec2 tr = DP_transform_xy(tf, x2, y1);
    DP_Vec2 br = DP_transform_xy(tf, x2, y2);
    DP_Vec2 bl = DP_transform_xy(tf, x1, y2);

    float *vertices = cr->attributes.vertices;
    vertices[0] = DP_double_to_float(bl.x);
    vertices[1] = DP_double_to_float(bl.y);
    vertices[2] = DP_double_to_float(tl.x);
    vertices[3] = DP_double_to_float(tl.y);
    vertices[4] = DP_double_to_float(br.x);
    vertices[5] = DP_double_to_float(br.y);
    vertices[6] = DP_double_to_float(tr.x);
    vertices[7] = DP_double_to_float(tr.y);
}

static void render_texture(DP_CanvasRenderer *cr, int view_height,
                           int view_width)
{
    DP_GL_CLEAR_ERROR();
    DP_GL(glEnable, GL_BLEND);
    DP_GL(glBlendFunc, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    DP_GL(glUseProgram, cr->program);
    DP_GL(glUniform2f, cr->uniforms.view, DP_int_to_float(view_width),
          DP_int_to_float(view_height));
    DP_GL(glUniform1i, cr->uniforms.sampler, 0);

    DP_GL(glEnableVertexAttribArray, 0);
    DP_GL(glBindBuffer, GL_ARRAY_BUFFER, cr->buffers[0]);
    DP_GL(glBufferData, GL_ARRAY_BUFFER, sizeof(cr->attributes.vertices),
          cr->attributes.vertices, GL_STATIC_DRAW);
    DP_GL(glVertexAttribPointer, 0, 2, GL_FLOAT, GL_FALSE, 0, NULL);

    DP_GL(glEnableVertexAttribArray, 1);
    DP_GL(glBindBuffer, GL_ARRAY_BUFFER, cr->buffers[1]);
    DP_GL(glBufferData, GL_ARRAY_BUFFER, sizeof(cr->attributes.uvs),
          cr->attributes.uvs, GL_STATIC_DRAW);
    DP_GL(glVertexAttribPointer, 1, 2, GL_FLOAT, GL_FALSE, 0, NULL);

    DP_GL(glDrawArrays, GL_TRIANGLE_STRIP, 0, 4);
}

void DP_canvas_renderer_render(DP_CanvasRenderer *cr, DP_LayerContent *lc,
                               int view_width, int view_height,
                               DP_CanvasDiff *diff_or_null)
{
    DP_ASSERT(cr);
    if (view_width <= 0 || view_height <= 0) {
        return; // No view to render to, bail out.
    }

    int layer_width = DP_layer_content_width(lc);
    int layer_height = DP_layer_content_height(lc);
    if (layer_width <= 0 || layer_height <= 0) {
        return; // Layer with zero dimension(s), bail out.
    }

    DP_GL(glActiveTexture, GL_TEXTURE0);
    DP_GL(glBindTexture, GL_TEXTURE_2D, cr->texture.id);
    if (resize(cr, layer_width, layer_height)) {
        write_all_tiles_to_texture(cr, lc, layer_width, layer_height);
    }
    else if (diff_or_null) {
        DP_canvas_diff_each_pos_reset(diff_or_null, write_diff_tile_to_texture,
                                      (void *[]){cr, lc});
    }

    if (cr->recalculate_vertices) {
        calculate_vertices(cr);
    }

    render_texture(cr, view_height, view_width);
}
