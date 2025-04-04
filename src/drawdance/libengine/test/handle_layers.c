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
#include "handle_common.h"
#include <dpengine/layer_content.h>
#include <dpengine/layer_group.h>
#include <dpengine/layer_list.h>
#include <dpengine/layer_props.h>
#include <dpengine/layer_props_list.h>
#include <dpmsg/blend_mode.h>


static void dump_layer_list(DP_Output *output, DP_LayerList *ll,
                            DP_LayerPropsList *lpl, int indent,
                            const char *prefix);

static void dump_layer_content(DP_Output *output, DP_LayerContent *lc,
                               DP_LayerProps *lp, int indent)
{
    format_indent(output, indent, "width: %d\n", DP_layer_content_width(lc));
    format_indent(output, indent, "height: %d\n", DP_layer_content_height(lc));
    if (DP_layer_props_children_noinc(lp)) {
        format_indent(output, indent,
                      "error: layer content properties has children\n");
    }
    dump_layer_list(output, DP_layer_content_sub_contents_noinc(lc),
                    DP_layer_content_sub_props_noinc(lc), indent, "sub");
}

static void dump_layer_group(DP_Output *output, DP_LayerGroup *lg,
                             DP_LayerProps *lp, int indent)
{
    format_indent(output, indent, "width: %d\n", DP_layer_group_width(lg));
    format_indent(output, indent, "height: %d\n", DP_layer_group_height(lg));
    DP_LayerPropsList *lpl = DP_layer_props_children_noinc(lp);
    if (lpl) {
        dump_layer_list(output, DP_layer_group_children_noinc(lg), lpl, indent,
                        "child ");
    }
    else {
        format_indent(output, indent,
                      "error: layer group properties has no children\n");
    }
}

static void dump_layer(DP_Output *output, DP_LayerListEntry *lle,
                       DP_LayerProps *lp, int indent)
{
    bool is_group = DP_layer_list_entry_is_group(lle);
    format_indent(output, indent, "type: %s\n", is_group ? "group" : "layer");
    format_indent(output, indent, "id: %d\n", DP_layer_props_id(lp));

    size_t title_length;
    const char *title = DP_layer_props_title(lp, &title_length);
    if (title) {
        format_indent(output, indent, "title: \"%.*s\"\n",
                      DP_size_to_int(title_length), title);
    }
    else {
        format_indent(output, indent, "title: NULL\n");
    }

    uint16_t opacity = DP_layer_props_opacity(lp);
    format_indent(output, indent, "opacity: %u (%.2f%%)\n",
                  DP_uint16_to_uint(opacity),
                  DP_uint16_to_double(opacity) / ((double)DP_BIT15) * 100.0);

    format_indent(
        output, indent, "blend mode: %s\n",
        DP_blend_mode_enum_name_unprefixed(DP_layer_props_blend_mode(lp)));
    format_indent(output, indent, "hidden: %s\n",
                  DP_layer_props_hidden(lp) ? "true" : "false");
    format_indent(output, indent, "censored: %s\n",
                  DP_layer_props_censored(lp) ? "true" : "false");
    format_indent(output, indent, "isolated: %s\n",
                  DP_layer_props_isolated(lp) ? "true" : "false");
    if (is_group) {
        dump_layer_group(output, DP_layer_list_entry_group_noinc(lle), lp,
                         indent);
    }
    else {
        dump_layer_content(output, DP_layer_list_entry_content_noinc(lle), lp,
                           indent);
    }
}

static void dump_layer_list(DP_Output *output, DP_LayerList *ll,
                            DP_LayerPropsList *lpl, int indent,
                            const char *prefix)
{
    int layer_count = DP_layer_list_count(ll);
    int layer_props_count = DP_layer_props_list_count(lpl);
    format_indent(output, indent, "%d %slayer(s), %d %slayer prop(s)\n",
                  layer_count, prefix, layer_props_count, prefix);
    if (layer_count == layer_props_count) {
        for (int i = 0; i < layer_count; ++i) {
            format_indent(output, indent, "[%d] = {\n", i);
            dump_layer(output, DP_layer_list_at_noinc(ll, i),
                       DP_layer_props_list_at_noinc(lpl, i), indent + 1);
            format_indent(output, indent, "}\n");
        }
    }
    else {
        write_indent(output, indent);
        DP_output_format(output, "error: layer count mismatch\n");
    }
}

static void dump_layers(DP_Output *output, DP_CanvasHistory *ch,
                        const char *title)
{
    DP_CanvasState *cs = DP_canvas_history_compare_and_get(ch, NULL, NULL);
    DP_output_format(output, "\n-- %s\n", title);
    dump_layer_list(output, DP_canvas_state_layers_noinc(cs),
                    DP_canvas_state_layer_props_noinc(cs), 0, "");
    DP_output_write(output, "\n", 1);
    DP_output_flush(output);
    DP_canvas_state_decref(cs);
}


static void add_canvas_resize(DP_Output *output, DP_CanvasHistory *ch,
                              DP_DrawContext *dc, int32_t top, int32_t right,
                              int32_t bottom, int32_t left)
{
    add_message(output, ch, dc,
                DP_msg_canvas_resize_new(1, top, right, bottom, left));
}

static void add_layer_create(DP_Output *output, DP_CanvasHistory *ch,
                             DP_DrawContext *dc, uint16_t id, uint16_t source,
                             uint16_t target, uint32_t fill, uint8_t flags,
                             const char *name)
{
    add_message(output, ch, dc,
                DP_msg_layer_tree_create_new(1, id, source, target, fill, flags,
                                             name ? name : "",
                                             name ? strlen(name) : 0));
}

static void add_layer_attributes(DP_Output *output, DP_CanvasHistory *ch,
                                 DP_DrawContext *dc, uint16_t id,
                                 uint8_t sublayer, uint8_t flags,
                                 uint8_t opacity, uint8_t blend)
{
    add_message(
        output, ch, dc,
        DP_msg_layer_attributes_new(1, id, sublayer, flags, opacity, blend));
}

static void add_layer_retitle(DP_Output *output, DP_CanvasHistory *ch,
                              DP_DrawContext *dc, uint16_t id,
                              const char *title)
{
    add_message(output, ch, dc,
                DP_msg_layer_retitle_new(1, id, title, strlen(title)));
}

static void add_layer_tree_delete(DP_Output *output, DP_CanvasHistory *ch,
                                  DP_DrawContext *dc, uint16_t id,
                                  uint16_t merge_to)
{
    add_message(output, ch, dc, DP_msg_layer_tree_delete_new(1, id, merge_to));
}

static void add_layer_tree_move(DP_Output *output, DP_CanvasHistory *ch,
                                DP_DrawContext *dc, uint16_t layer,
                                uint16_t parent, uint16_t sibling)
{
    add_message(output, ch, dc,
                DP_msg_layer_tree_move_new(1, layer, parent, sibling));
}

static void set_pixel_dab(DP_UNUSED int count, DP_PixelDab *dabs,
                          DP_UNUSED void *user)
{
    DP_ASSERT(count == 1);
    DP_pixel_dab_init(dabs, 0, 0, 0, 1, 255);
}

static void add_draw_dabs_pixel_square(DP_Output *output, DP_CanvasHistory *ch,
                                       DP_DrawContext *dc, uint8_t flags,
                                       uint16_t layer, int32_t x, int32_t y,
                                       uint32_t color, uint8_t mode)
{
    add_message(output, ch, dc,
                DP_msg_draw_dabs_pixel_square_new(1, flags, layer, x, y, color,
                                                  mode, set_pixel_dab, 1,
                                                  NULL));
}

static void add_pen_up(DP_Output *output, DP_CanvasHistory *ch,
                       DP_DrawContext *dc)
{
    add_message(output, ch, dc, DP_msg_pen_up_new(1));
}


static void handle_layers(DP_Output *output, DP_CanvasHistory *ch,
                          DP_DrawContext *dc)
{
    dump_layers(output, ch, "initial layers");

    add_undo_point(output, ch, dc);
    add_layer_create(output, ch, dc, 258, 0, 0, 0,
                     DP_MSG_LAYER_TREE_CREATE_FLAGS_GROUP, "Group 1");
    dump_layers(output, ch, "create initial group");

    add_undo_point(output, ch, dc);
    add_layer_create(output, ch, dc, 257, 0, 0, 0, 0, "Layer 1");
    dump_layers(output, ch, "create initial layer");

    add_undo_point(output, ch, dc);
    add_layer_create(output, ch, dc, 259, 0, 258, 0,
                     DP_MSG_LAYER_TREE_CREATE_FLAGS_INTO, "Layer 2");
    dump_layers(output, ch, "create layer in group");

    add_undo_point(output, ch, dc);
    add_layer_create(output, ch, dc, 260, 0, 259, 0,
                     DP_MSG_LAYER_TREE_CREATE_FLAGS_GROUP, "Group 2");
    dump_layers(output, ch, "create group in group");

    add_undo_point(output, ch, dc);
    add_layer_attributes(output, ch, dc, 257, 0,
                         DP_MSG_LAYER_ATTRIBUTES_FLAGS_CENSOR, 255,
                         DP_BLEND_MODE_MULTIPLY);
    dump_layers(output, ch, "change layer attributes");

    add_undo_point(output, ch, dc);
    add_layer_create(output, ch, dc, 261, 257, 260, 0,
                     DP_MSG_LAYER_TREE_CREATE_FLAGS_INTO, "Layer 1 Copy");
    dump_layers(output, ch, "create layer duplicate in inner group");

    add_undo_point(output, ch, dc);
    add_layer_retitle(output, ch, dc, 261, "Copy of Layer 1");
    dump_layers(output, ch, "rename layer");

    add_undo_point(output, ch, dc);
    add_canvas_resize(output, ch, dc, 0, 16, 9, 0);
    dump_layers(output, ch, "resize layers");

    add_undo_point(output, ch, dc);
    add_draw_dabs_pixel_square(output, ch, dc, (uint8_t)DP_PAINT_MODE_DIRECT,
                               261, 0, 0, 0x00abcdef, DP_BLEND_MODE_SUBTRACT);
    dump_layers(output, ch, "draw dab in direct mode");
    add_pen_up(output, ch, dc);
    dump_layers(output, ch, "pen up in direct mode");

    add_undo_point(output, ch, dc);
    add_draw_dabs_pixel_square(output, ch, dc,
                               (uint8_t)DP_PAINT_MODE_INDIRECT_WASH, 261, 0, 0,
                               0x7fabcdef, DP_BLEND_MODE_SCREEN);
    dump_layers(output, ch, "draw dab in indirect mode");
    add_pen_up(output, ch, dc);
    dump_layers(output, ch, "pen up in indirect mode");

    add_undo_point(output, ch, dc);
    add_layer_tree_move(output, ch, dc, 0, 0, 0);
    add_layer_tree_move(output, ch, dc, 257, 257, 0);
    add_layer_tree_move(output, ch, dc, 257, 0, 257);
    add_layer_tree_move(output, ch, dc, 257, 258, 258);
    add_layer_tree_move(output, ch, dc, 111, 0, 0);
    add_layer_tree_move(output, ch, dc, 257, 222, 0);
    add_layer_tree_move(output, ch, dc, 257, 0, 333);
    add_layer_tree_move(output, ch, dc, 258, 260, 0);
    add_layer_tree_move(output, ch, dc, 257, 261, 0);
    add_layer_tree_move(output, ch, dc, 257, 260, 258);
    dump_layers(output, ch, "invalid layer tree moves");
    add_undo(output, ch, dc);

    add_undo_point(output, ch, dc);
    add_layer_tree_move(output, ch, dc, 257, 0, 258);
    dump_layers(output, ch, "swap layers in root");
    add_layer_tree_move(output, ch, dc, 260, 0, 0);
    dump_layers(output, ch, "move layer out of group");
    add_layer_tree_move(output, ch, dc, 259, 0, 260);
    dump_layers(output, ch, "move layer out of nested group");
    add_undo(output, ch, dc);

    add_undo_point(output, ch, dc);
    add_layer_tree_move(output, ch, dc, 259, 260, 261);
    dump_layers(output, ch, "move layer into group");
    add_undo(output, ch, dc);

    add_undo_point(output, ch, dc);
    add_layer_create(output, ch, dc, 262, 258, 260, 0,
                     DP_MSG_LAYER_TREE_CREATE_FLAGS_INTO, "Group 1 Copy");
    dump_layers(output, ch, "create group duplicate in inner group");
    add_undo(output, ch, dc);

    add_undo_point(output, ch, dc);
    add_layer_tree_delete(output, ch, dc, 259, 257);
    dump_layers(output, ch, "merge layer");
    add_undo(output, ch, dc);

    add_undo_point(output, ch, dc);
    add_layer_tree_delete(output, ch, dc, 260, 259);
    dump_layers(output, ch, "merge group");
    add_undo(output, ch, dc);

    add_undo_point(output, ch, dc);
    add_layer_tree_delete(output, ch, dc, 257, 0);
    dump_layers(output, ch, "delete layer in root");

    add_undo_point(output, ch, dc);
    add_layer_tree_delete(output, ch, dc, 261, 0);
    dump_layers(output, ch, "delete nested layer");

    add_undo_point(output, ch, dc);
    add_layer_tree_delete(output, ch, dc, 258, 0);
    dump_layers(output, ch, "delete group");
}


int main(int argc, char **argv)
{
    static DP_HandleTest tests[] = {
        {"handle_layers", "test/tmp/handle_layers", "test/data/handle_layers",
         handle_layers},
        {NULL, NULL, NULL, NULL},
    };
    return DP_test_main(argc, argv, register_handle_tests, tests);
}
