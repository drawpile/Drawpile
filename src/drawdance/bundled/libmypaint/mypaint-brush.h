#ifndef MYPAINTBRUSH_H
#define MYPAINTBRUSH_H

/* libmypaint - The MyPaint Brush Library
 * Copyright (C) 2008 Martin Renold <martinxyz@gmx.ch>
 * Copyright (C) 2012 Jon Nordby <jononor@gmail.com>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "mypaint-config.h"
#include "mypaint-surface.h"
#include "mypaint-brush-settings.h"

G_BEGIN_DECLS
/*!
 * @struct MyPaintBrush
 * Maintains the data and state required to perform brush strokes.
 *
 * This is the hub and core of the brush engine, holding the settings,
 * mappings and state that is used to calculate the location, and other
 * properties, of the dabs that make up painted strokes.
 *
 * The brush is an opaque struct using manual reference counting and handles
 * its own destruction based on that.
 * See #mypaint_brush_ref and #mypaint_brush_unref.
 */
typedef struct MyPaintBrush MyPaintBrush;

/*!
 * Create a new brush
 *
 * @memberof MyPaintBrush
 */
MyPaintBrush *
mypaint_brush_new(void);

/*!
 * Create a new brush with smudge buckets enabled.
 *
 * This kind of brush is required to make use of the
 * @ref MYPAINT_BRUSH_SETTING_SMUDGE_BUCKET setting.
 *
 * Smudge buckets are an array of smudge data states, used to allow
 * dabs in offset locations to use smudge data consistent with their location,
 * when those location-sequential dabs are not actually drawn consecutively.
 *
 * @memberof MyPaintBrush
 */
MyPaintBrush *
mypaint_brush_new_with_buckets(int num_smudge_buckets);

/*!
 * Decrease the reference count by one, destroying the brush if it reaches 0.
 *
 * Not safe to call with NULL!
 *
 * @memberof MyPaintBrush
 */
void
mypaint_brush_unref(MyPaintBrush *self);

/*!
 * Increase the reference count by one.
 *
 * Not safe to call with NULL!
 *
 * @memberof MyPaintBrush
 */
void
mypaint_brush_ref(MyPaintBrush *self);

/*!
 * Request a reset if the brush state.
 *
 * The request is not acted on immediately, but only in subsequent calls
 * to mypaint_brush_stroke_to and/or mypaint_brush_stroke_to2.
 *
 * Only state values (including smudge buckets, if used) are affected upon reset,
 * not settings/mappings.
 *
 * @memberof MyPaintBrush
 */
void
mypaint_brush_reset(MyPaintBrush *self);

/*!
 * Start a new stroke.
 *
 * Reset the stroke time parameters.
 * Not directly related to mypaint_stroke_to (and varieties), calling
 * the former does not require calling mypaint_brush_new_stroke.
 *
 * @memberof MyPaintBrush
 */
void
mypaint_brush_new_stroke(MyPaintBrush *self);

/*!
 * Use the brush to draw a stroke segment on a MyPaintSurface
 *
 * @memberof MyPaintBrush
 */
int
mypaint_brush_stroke_to(
    MyPaintBrush* self, MyPaintSurface* surface, float x, float y, float pressure, float xtilt, float ytilt,
    double dtime);

/*!
 * Use the brush to draw a stroke segment on a MyPaintSurface2
 *
 * @memberof MyPaintBrush
 */
int
mypaint_brush_stroke_to_2(
    MyPaintBrush* self, MyPaintSurface2* surface, float x, float y, float pressure, float xtilt, float ytilt,
    double dtime, float viewzoom, float viewrotation, float barrel_rotation);

/*!
 * Same as mypaint_brush_stroke_to_2, but using linear sRGB for color dynamics.
 *
 * The settings that are handled differently from the other call are:
 *
 * - ::MYPAINT_BRUSH_SETTING_CHANGE_COLOR_H
 * - ::MYPAINT_BRUSH_SETTING_CHANGE_COLOR_L
 * - ::MYPAINT_BRUSH_SETTING_CHANGE_COLOR_HSL_S
 * - ::MYPAINT_BRUSH_SETTING_CHANGE_COLOR_V
 * - ::MYPAINT_BRUSH_SETTING_CHANGE_COLOR_HSV_S
 *
 * @memberof MyPaintBrush
 */
int
mypaint_brush_stroke_to_2_linearsRGB(
    MyPaintBrush* self, MyPaintSurface2* surface, float x, float y, float pressure, float xtilt, float ytilt,
    double dtime, float viewzoom, float viewrotation, float barrel_rotation);

/*!
 * Set the base value of a brush setting.
 *
 * @memberof MyPaintBrush
 */
void
mypaint_brush_set_base_value(MyPaintBrush *self, MyPaintBrushSetting id, float value);

/*!
 * Get the base value of a brush setting.
 *
 * @memberof MyPaintBrush
 */
float
mypaint_brush_get_base_value(MyPaintBrush *self, MyPaintBrushSetting id);

/*!
 * Check if there are no dynamics/mappings for a brush setting.
 *
 * This will return FALSE if any input mappings exist for the setting with
 * the given id, even if all such mappings themselves are constant.
 *
 * @memberof MyPaintBrush
 */
gboolean
mypaint_brush_is_constant(MyPaintBrush *self, MyPaintBrushSetting id);

/*!
 * Get the number of input mappings for a brush setting.
 *
 * @memberof MyPaintBrush
 */
int
mypaint_brush_get_inputs_used_n(MyPaintBrush *self, MyPaintBrushSetting id);

/*!
 * Set the number of points in an input mapping for a brush setting.
 *
 * @memberof MyPaintBrush
 */
void
mypaint_brush_set_mapping_n(MyPaintBrush *self, MyPaintBrushSetting id, MyPaintBrushInput input, int n);

/*!
 * Get the number of points in an input mapping for a brush setting.
 *
 * @memberof MyPaintBrush
 */
int
mypaint_brush_get_mapping_n(MyPaintBrush *self, MyPaintBrushSetting id, MyPaintBrushInput input);

/*!
 * Set the coordinates of a point in an input mapping for a brush setting.
 *
 * @memberof MyPaintBrush
 */
void
mypaint_brush_set_mapping_point(MyPaintBrush *self, MyPaintBrushSetting id, MyPaintBrushInput input, int index, float x, float y);

/*!
 * Get the coordinates of a point in an input mapping for a brush setting.
 *
 * @memberof MyPaintBrush
 */
void
mypaint_brush_get_mapping_point(MyPaintBrush *self, MyPaintBrushSetting id, MyPaintBrushInput input, int index, float *x, float *y);

/*!
 * Get the value of a brush state.
 *
 * @memberof MyPaintBrush
 */
float
mypaint_brush_get_state(MyPaintBrush *self, MyPaintBrushState i);

/*!
 * Set the value of a brush state.
 *
 * @memberof MyPaintBrush
 */
void
mypaint_brush_set_state(MyPaintBrush *self, MyPaintBrushState i, float value);

/*!
 * Get the total time recorded since the last call to mypaint_brush_reset
 *
 * @memberof MyPaintBrush
 */
double
mypaint_brush_get_total_stroke_painting_time(MyPaintBrush *self);

/*!
 * Enable/Disable debug printouts
 *
 * @memberof MyPaintBrush
 */
void
mypaint_brush_set_print_inputs(MyPaintBrush *self, gboolean enabled);

/*!
 * Initialize a brush with default values for all settings.
 *
 * @memberof MyPaintBrush
 */
void
mypaint_brush_from_defaults(MyPaintBrush *self);


/*!
 * Initialize a brush from a JSON string.
 *
 * @memberof MyPaintBrush
 */
gboolean
mypaint_brush_from_string(MyPaintBrush *self, const char *string);

// Drawpile patch: grab the RNG so we don't need to instantiate another one.
typedef struct RngDouble RngDouble;
RngDouble *
mypaint_brush_rng(MyPaintBrush *self);


G_END_DECLS

#endif // MYPAINTBRUSH_H
