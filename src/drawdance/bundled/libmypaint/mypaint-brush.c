/* libmypaint - The MyPaint Brush Library
 * Copyright (C) 2007-2011 Martin Renold <martinxyz@gmx.ch>
 *
 * With Drawpile patches to disable an include of a useless glib file and JSON
 * support. We don't need glib and our JSON is loaded through Qt, so we can
 * avoid the dependency on the json-c library. The patches are marked with
 * DRAWPILE_UNWANTED_MYPAINT_FEATURES.
 *
 * There's also a fix for an off by one error when clearing smudge buckets.
 * That has been marked with "Drawpile Patch".
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

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <assert.h>

#if MYPAINT_CONFIG_USE_GLIB
#include <glib.h>
#include "glib/mypaint-brush.h"
#endif

#include "mypaint-brush.h"

#include "mypaint-brush-settings.h"
#include "mypaint-mapping.h"
#include "helpers.h"
#include "rng-double.h"

#ifdef DRAWPILE_UNWANTED_MYPAINT_FEATURES
#include <json.h>
#endif

#ifdef _MSC_VER
#if _MSC_VER < 1700     // Visual Studio 2012 and later has isfinite and roundf
  #include <float.h>
  static inline int    isfinite(double x) { return _finite(x); }
  static inline float  roundf  (float  x) { return x >= 0.0f ? floorf(x + 0.5f) : ceilf(x - 0.5f); }
#endif
#endif

// Conversion from degrees to radians
#define RADIANS(x) ((x) * M_PI / 180.0)
// Conversion from radians to degrees
#define DEGREES(x) (((x) / (2 * M_PI)) * 360.0)

#define ACTUAL_RADIUS_MIN 0.2
#define ACTUAL_RADIUS_MAX 1000 // safety guard against radius like 1e20 and against rendering overload with unexpected brush dynamics

#define GRID_SIZE 256.0

/* Named indices for the smudge bucket arrays */
enum {
  SMUDGE_R, SMUDGE_G, SMUDGE_B, SMUDGE_A,
  PREV_COL_R, PREV_COL_G, PREV_COL_B, PREV_COL_A,
  PREV_COL_RECENTNESS,
  SMUDGE_BUCKET_SIZE
};

/* The Brush class stores two things:
   b) settings: constant during a stroke (eg. size, spacing, dynamics, color selected by the user)
   a) states: modified during a stroke (eg. speed, smudge colors, time/distance to next dab, position filter states)

   FIXME: Actually those are two orthogonal things. Should separate them:
          a) brush settings class that is saved/loaded/selected  (without states)
          b) brush core class to draw the dabs (using an instance of the above)

   In python, there are two kinds of instances from this: a "global
   brush" which does the cursor tracking, and the "brushlist" where
   the states are ignored. When a brush is selected, its settings are
   copied into the global one, leaving the state intact.
 */


/**
  * MyPaintBrush:
  *
  * The MyPaint brush engine class.
  */
struct MyPaintBrush {

    gboolean print_inputs; // debug menu
    // for stroke splitting (undo/redo)
    double stroke_total_painting_time;
    double stroke_current_idling_time;

    // the states (get_state, set_state, reset) that change during a stroke
    float states[MYPAINT_BRUSH_STATES_COUNT];
    // smudge bucket array: part of the state, but stored separately.
    // Usually used for brushes with multiple offset dabs, where each
    // dab is assigned its own bucket containing a smudge state.
    float *smudge_buckets;
    int num_buckets;
    int min_bucket_used;
    int max_bucket_used;

    double random_input;
    float skip;
    float skip_last_x;
    float skip_last_y;
    float skipped_dtime;
    RngDouble * rng;

    // Those mappings describe how to calculate the current value for each setting.
    // Most of settings will be constant (eg. only their base_value is used).
    MyPaintMapping * settings[MYPAINT_BRUSH_SETTINGS_COUNT];

    // the current value of all settings (calculated using the current state)
    float settings_value[MYPAINT_BRUSH_SETTINGS_COUNT];

    // see also brushsettings.py

    // cached calculation results
    float speed_mapping_gamma[2];
    float speed_mapping_m[2];
    float speed_mapping_q[2];

    gboolean reset_requested;
#ifdef DRAWPILE_UNWANTED_MYPAINT_FEATURES
    json_object *brush_json;
#endif
    int refcount;
};

/* Macros for accessing states and setting values
   Although macros are never nice, simple file-local macros are warranted
   here since it massively improves code readability.
   The INPUT macro relies on 'inputs' being a float array of size MYPAINT_BRUSH_INPUTS_COUNT,
   accessible in the scope where the macro is used.
*/
#define STATE(self, state_name) ((self)->states[MYPAINT_BRUSH_STATE_##state_name])
#define SETTING(self, setting_name) ((self)->settings_value[MYPAINT_BRUSH_SETTING_##setting_name])
#define BASEVAL(self, setting_name) (mypaint_mapping_get_base_value((self)->settings[MYPAINT_BRUSH_SETTING_##setting_name]))
#define INPUT(input_name) (inputs[MYPAINT_BRUSH_INPUT_##input_name])

void settings_base_values_have_changed (MyPaintBrush *self);

#ifdef DRAWPILE_UNWANTED_MYPAINT_FEATURES
#include "glib/mypaint-brush.c"
#endif

void
brush_reset(MyPaintBrush *self)
{
    self->skip = 0;
    self->skip_last_x = 0;
    self->skip_last_y = 0;
    self->skipped_dtime = 0;
    // Clear states
    memset(self->states, 0, sizeof(self->states));
    // Set the flip state such that it will be at "1" for the first
    // dab, and then switch between -1 and 1 for the subsequent dabs.
    STATE(self, FLIP) = -1;
    // Clear smudge buckets
    if (self->smudge_buckets) {
      int min_index = self->min_bucket_used;
      if (min_index != -1) {
        int max_index = self->max_bucket_used;
        // Drawpile Patch: fix off by one error when clearing smudge buckets.
        // We added the + 1 here, since max_index is *inclusive*. See also
        // https://github.com/mypaint/libmypaint/pull/186
        size_t num_bytes = (max_index - min_index + 1) * sizeof(self->smudge_buckets[0]) * SMUDGE_BUCKET_SIZE;
        memset(self->smudge_buckets + min_index, 0, num_bytes);
        self->min_bucket_used = -1;
        self->max_bucket_used = -1;
      }
    }
}

/**
  * mypaint_brush_new:
  *
  * Create a new MyPaint brush engine instance.
  * Initial reference count is 1. Release references using mypaint_brush_unref()
  */
MyPaintBrush *
mypaint_brush_new(void)
{
  return mypaint_brush_new_with_buckets(0);
}

MyPaintBrush *
mypaint_brush_new_with_buckets(int num_smudge_buckets)
{
    MyPaintBrush *self = (MyPaintBrush *)malloc(sizeof(MyPaintBrush));

    if (!self) {
      return NULL;
    }

    if (num_smudge_buckets > 0) {
      float *bucket_array = malloc(num_smudge_buckets * SMUDGE_BUCKET_SIZE * sizeof(float));
      if (!bucket_array) {
        free(self);
        return NULL;
      }
      self->smudge_buckets = bucket_array;
      self->num_buckets = num_smudge_buckets;
      // Set up min/max to initialize (clear) the array in the call to brush_reset.
      self->min_bucket_used = 0;
      self->max_bucket_used = self->num_buckets - 1;
    } else {
      self->smudge_buckets = NULL;
      self->num_buckets = 0;
    }

    self->refcount = 1;
    for (int i = 0; i < MYPAINT_BRUSH_SETTINGS_COUNT; i++) {
      self->settings[i] = mypaint_mapping_new(MYPAINT_BRUSH_INPUTS_COUNT);
    }
    self->rng = rng_double_new(1000);
    self->random_input = 0;
    self->print_inputs = FALSE;

    brush_reset(self);

    mypaint_brush_new_stroke(self);
    settings_base_values_have_changed(self);

    self->reset_requested = TRUE;
#ifdef DRAWPILE_UNWANTED_MYPAINT_FEATURES
    self->brush_json = json_object_new_object();
#endif
    return self;
}

void
brush_free(MyPaintBrush *self)
{
    for (int i = 0; i < MYPAINT_BRUSH_SETTINGS_COUNT; i++) {
        mypaint_mapping_free(self->settings[i]);
    }
    rng_double_free (self->rng);
    self->rng = NULL;
#ifdef DRAWPILE_UNWANTED_MYPAINT_FEATURES
    if (self->brush_json) {
        json_object_put(self->brush_json);
    }
#endif
    free(self->smudge_buckets);
    free(self);
}

/**
  * mypaint_brush_unref: (skip)
  *
  * Decrease the reference count. Will be freed when it hits 0.
  */
void
mypaint_brush_unref(MyPaintBrush *self)
{
    self->refcount--;
    if (self->refcount == 0) {
        brush_free(self);
    }
}
/**
  * mypaint_brush_ref: (skip)
  *
  * Increase the reference count.
  */
void
mypaint_brush_ref(MyPaintBrush *self)
{
    self->refcount++;
}

/**
  * mypaint_brush_get_total_stroke_painting_time:
  *
  * Return the total amount of painting time for the current stroke.
  */
double
mypaint_brush_get_total_stroke_painting_time(MyPaintBrush *self)
{
    return self->stroke_total_painting_time;
}

/**
  * mypaint_brush_set_print_inputs:
  *
  * Enable/Disable printing of brush engine inputs on stderr. Intended for debugging only.
  */
void
mypaint_brush_set_print_inputs(MyPaintBrush *self, gboolean enabled)
{
    self->print_inputs = enabled;
}

/**
  * mypaint_brush_reset:
  *
  * Reset the current brush engine state.
  * Used when the next mypaint_brush_stroke_to() call is not related to the current state.
  * Note that the reset request is queued and changes in state will only happen on next stroke_to()
  */
void
mypaint_brush_reset(MyPaintBrush *self)
{
    self->reset_requested = TRUE;
}

/**
  * mypaint_brush_new_stroke:
  *
  * Start a new stroke.
  */
void
mypaint_brush_new_stroke(MyPaintBrush *self)
{
    self->stroke_current_idling_time = 0;
    self->stroke_total_painting_time = 0;
}

/**
  * mypaint_brush_set_base_value:
  *
  * Set the base value of a brush setting.
  */
void
mypaint_brush_set_base_value(MyPaintBrush *self, MyPaintBrushSetting id, float value)
{
    assert (id < MYPAINT_BRUSH_SETTINGS_COUNT);
    mypaint_mapping_set_base_value(self->settings[id], value);

    settings_base_values_have_changed (self);
}

/**
  * mypaint_brush_get_base_value:
  *
  * Get the base value of a brush setting.
  */
float
mypaint_brush_get_base_value(MyPaintBrush *self, MyPaintBrushSetting id)
{
    assert (id < MYPAINT_BRUSH_SETTINGS_COUNT);
    return mypaint_mapping_get_base_value(self->settings[id]);
}

/**
  * mypaint_brush_set_mapping_n:
  *
  * Set the number of points used for the dynamics mapping between a #MyPaintBrushInput and #MyPaintBrushSetting.
  */
void
mypaint_brush_set_mapping_n(MyPaintBrush *self, MyPaintBrushSetting id, MyPaintBrushInput input, int n)
{
    assert (id < MYPAINT_BRUSH_SETTINGS_COUNT);
    mypaint_mapping_set_n(self->settings[id], input, n);
}

/**
  * mypaint_brush_get_mapping_n:
  *
  * Get the number of points used for the dynamics mapping between a #MyPaintBrushInput and #MyPaintBrushSetting.
  */
int
mypaint_brush_get_mapping_n(MyPaintBrush *self, MyPaintBrushSetting id, MyPaintBrushInput input)
{
    return mypaint_mapping_get_n(self->settings[id], input);
}

/**
  * mypaint_brush_is_constant:
  *
  * Returns TRUE if the brush has no dynamics for the given #MyPaintBrushSetting
  */
gboolean
mypaint_brush_is_constant(MyPaintBrush *self, MyPaintBrushSetting id)
{
    assert (id < MYPAINT_BRUSH_SETTINGS_COUNT);
    return mypaint_mapping_is_constant(self->settings[id]);
}

/**
  * mypaint_brush_get_inputs_used_n:
  *
  * Returns how many inputs are used for the dynamics of a #MyPaintBrushSetting
  */
int
mypaint_brush_get_inputs_used_n(MyPaintBrush *self, MyPaintBrushSetting id)
{
    assert (id < MYPAINT_BRUSH_SETTINGS_COUNT);
    return mypaint_mapping_get_inputs_used_n(self->settings[id]);
}

/**
  * mypaint_brush_set_mapping_point:
  *
  * Set a X,Y point of a dynamics mapping.
  * The index must be within the number of points set using mypaint_brush_set_mapping_n()
  */
void
mypaint_brush_set_mapping_point(MyPaintBrush *self, MyPaintBrushSetting id, MyPaintBrushInput input, int index, float x, float y)
{
    assert (id < MYPAINT_BRUSH_SETTINGS_COUNT);
    mypaint_mapping_set_point(self->settings[id], input, index, x, y);
}

/**
 * mypaint_brush_get_mapping_point:
 * @x: (out): Location to return the X value
 * @y: (out): Location to return the Y value
 *
 * Get a X,Y point of a dynamics mapping.
 **/
void
mypaint_brush_get_mapping_point(MyPaintBrush *self, MyPaintBrushSetting id, MyPaintBrushInput input, int index, float *x, float *y)
{
    assert (id < MYPAINT_BRUSH_SETTINGS_COUNT);
    mypaint_mapping_get_point(self->settings[id], input, index, x, y);
}

/**
 * mypaint_brush_get_state:
 *
 * Get an internal brush engine state.
 * Normally used for debugging, but can be used to implement record & replay functionality.
 **/
float
mypaint_brush_get_state(MyPaintBrush *self, MyPaintBrushState i)
{
    assert (i < MYPAINT_BRUSH_STATES_COUNT);
    return self->states[i];
}

/**
 * mypaint_brush_set_state:
 *
 * Set an internal brush engine state.
 * Normally used for debugging, but can be used to implement record & replay functionality.
 **/
void
mypaint_brush_set_state(MyPaintBrush *self, MyPaintBrushState i, float value)
{
    assert (i < MYPAINT_BRUSH_STATES_COUNT);
    self->states[i] = value;
}

  // returns the fraction still left after t seconds
  float exp_decay (float T_const, float t)
  {
    // the argument might not make mathematical sense (whatever.)
    if (T_const <= 0.001) {
      return 0.0;
    }

    const float arg = -t / T_const;
    return expf(arg);
  }


  void settings_base_values_have_changed (MyPaintBrush *self)
  {
    // precalculate stuff that does not change dynamically

    // Precalculate how the physical speed will be mapped to the speed input value.
    // The formula for this mapping is:
    //
    // y = log(gamma+x)*m + q;
    //
    // x: the physical speed (pixels per basic dab radius)
    // y: the speed input that will be reported
    // gamma: parameter set by the user (small means a logarithmic mapping, big linear)
    // m, q: parameters to scale and translate the curve
    //
    // The code below calculates m and q given gamma and two hardcoded constraints.
    //
    for (int i = 0; i < 2; i++) {
      const float gamma = expf(i == 0 ? BASEVAL(self, SPEED1_GAMMA) : BASEVAL(self, SPEED2_GAMMA));

      const float fix1_x = 45.0;
      const float fix1_y = 0.5;
      const float fix2_x = 45.0;
      const float fix2_dy = 0.015;

      const float c1 = log(fix1_x + gamma);
      const float m = fix2_dy * (fix2_x + gamma);
      const float q = fix1_y - m * c1;

      self->speed_mapping_gamma[i] = gamma;
      self->speed_mapping_m[i] = m;
      self->speed_mapping_q[i] = q;
    }
  }

typedef struct {
  float x;
  float y;
} Offsets;

Offsets
directional_offsets(const MyPaintBrush* const self, const float base_radius, const int brush_flip)
{
    const float offset_mult = expf(SETTING(self, OFFSET_MULTIPLIER));
    // Sanity check - it is easy to reach infinite multipliers w. logarithmic parameters
    if (!isfinite(offset_mult)) {
        Offsets offs = {0.0f, 0.0f};
        return offs;
    }

    float dx = SETTING(self, OFFSET_X);
    float dy = SETTING(self, OFFSET_Y);

    //Anti_Art offsets tweaked by BrienD.  Adjusted with ANGLE_ADJ and OFFSET_MULTIPLIER
    const float offset_angle_adj = SETTING(self, OFFSET_ANGLE_ADJ);
    const float dir_angle_dy = STATE(self, DIRECTION_ANGLE_DY);
    const float dir_angle_dx = STATE(self, DIRECTION_ANGLE_DX);
    const float angle_deg = fmodf(DEGREES(atan2f(dir_angle_dy, dir_angle_dx)) - 90, 360);

    //offset to one side of direction
    const float offset_angle = SETTING(self, OFFSET_ANGLE);
    if (offset_angle) {
        const float dir_angle = RADIANS(angle_deg + offset_angle_adj);
        dx += cos(dir_angle) * offset_angle;
        dy += sin(dir_angle) * offset_angle;
    }

    //offset to one side of ascension angle
    const float view_rotation = STATE(self, VIEWROTATION);
    const float offset_angle_asc = SETTING(self, OFFSET_ANGLE_ASC);
    if (offset_angle_asc) {
        const float ascension = STATE(self, ASCENSION);
        const float asc_angle = RADIANS(ascension - view_rotation + offset_angle_adj);
        dx += cos(asc_angle) * offset_angle_asc;
        dy += sin(asc_angle) * offset_angle_asc;
      }

    //offset to one side of view orientation
    const float view_offset = SETTING(self, OFFSET_ANGLE_VIEW);
    if (view_offset) {
        const float view_angle = RADIANS(view_rotation + offset_angle_adj);
        dx += cos(-view_angle) * view_offset;
        dy += sin(-view_angle) * view_offset;
    }

    //offset mirrored to sides of direction
    const float offset_dir_mirror = MAX(0.0, SETTING(self, OFFSET_ANGLE_2));
    if (offset_dir_mirror) {
        const float dir_mirror_angle = RADIANS(angle_deg + offset_angle_adj * brush_flip);
        const float offset_factor = offset_dir_mirror * brush_flip;
        dx += cos(dir_mirror_angle) * offset_factor;
        dy += sin(dir_mirror_angle) * offset_factor;
    }

    //offset mirrored to sides of ascension angle
    const float offset_asc_mirror = MAX(0.0, SETTING(self, OFFSET_ANGLE_2_ASC));
    if (offset_asc_mirror) {
        const float ascension = STATE(self, ASCENSION);
        const float asc_angle = RADIANS(ascension - view_rotation + offset_angle_adj * brush_flip);
        const float offset_factor = brush_flip * offset_asc_mirror;
        dx += cos(asc_angle) * offset_factor;
        dy += sin(asc_angle) * offset_factor;
    }

    //offset mirrored to sides of view orientation
    const float offset_view_mirror = MAX(0.0, SETTING(self, OFFSET_ANGLE_2_VIEW));
    if (offset_view_mirror) {
        const float offset_factor = brush_flip * offset_view_mirror;
        const float offset_angle_rad = RADIANS(view_rotation + offset_angle_adj);
        dx += cos(-offset_angle_rad) * offset_factor;
        dy += sin(-offset_angle_rad) * offset_factor;
    }
    // Clamp the final offsets to avoid potential memory issues (extreme memory use from redraws)
    // Allow offsets up to the 1080 * 3 pixels. Unlikely to hamper anyone artistically.
    const float lim = 3240;
    const float base_mul = base_radius * offset_mult;
    Offsets offs = {CLAMP(dx * base_mul, -lim, lim),  CLAMP(dy * base_mul, -lim, lim)};
    return offs;
}

// Debugging: print brush inputs/states (not all of them)
void print_inputs(MyPaintBrush *self, float* inputs)
{
    printf(
        "press=% 4.3f, speed1=% 4.4f\tspeed2=% 4.4f",
        INPUT(PRESSURE),
        INPUT(SPEED1),
        INPUT(SPEED2)
        );
    printf(
        "\tstroke=% 4.3f\tcustom=% 4.3f",
        INPUT(STROKE),
        INPUT(CUSTOM)
        );
    printf(
        "\tviewzoom=% 4.3f\tviewrotation=% 4.3f",
        INPUT(VIEWZOOM),
        STATE(self, VIEWROTATION)
        );
    printf(
        "\tasc=% 4.3f\tdir=% 4.3f\tdec=% 4.3f\tdabang=% 4.3f",
        INPUT(TILT_ASCENSION),
        INPUT(DIRECTION),
        INPUT(TILT_DECLINATION),
        STATE(self, ACTUAL_ELLIPTICAL_DAB_ANGLE)
        );
    printf(
        "\txtilt=% 4.3f\tytilt=% 4.3fattack=% 4.3f",
        INPUT(TILT_DECLINATIONX),
        INPUT(TILT_DECLINATIONY),
        INPUT(ATTACK_ANGLE)
        );
    printf("\n");
}

  // This function runs a brush "simulation" step. Usually it is
  // called once or twice per dab. In theory the precision of the
  // "simulation" gets better when it is called more often. In
  // practice this only matters if there are some highly nonlinear
  // mappings in critical places or extremely few events per second.
  //
  // note: parameters are is dx/ddab, ..., dtime/ddab (dab is the number, 5.0 = 5th dab)
  void update_states_and_setting_values (MyPaintBrush *self, float step_ddab, float step_dx, float step_dy, float step_dpressure, float step_declination, float step_ascension, float step_dtime, float step_viewzoom, float step_viewrotation, float step_declinationx, float step_declinationy, float step_barrel_rotation)
  {
    if (step_dtime < 0.0) {
      printf("Time is running backwards!\n");
      step_dtime = 0.001;
    } else if (step_dtime == 0.0) {
      // FIXME: happens about every 10th start, workaround (against division by zero)
      step_dtime = 0.001;
    }

    STATE(self, X) += step_dx;
    STATE(self, Y) += step_dy;
    STATE(self, PRESSURE) += step_dpressure;

    STATE(self, DABS_PER_BASIC_RADIUS) = SETTING(self, DABS_PER_BASIC_RADIUS);
    STATE(self, DABS_PER_ACTUAL_RADIUS) = SETTING(self, DABS_PER_ACTUAL_RADIUS);
    STATE(self, DABS_PER_SECOND) = SETTING(self, DABS_PER_SECOND);

    STATE(self, DECLINATION) += step_declination;
    STATE(self, ASCENSION) += step_ascension;
    STATE(self, DECLINATIONX) += step_declinationx;
    STATE(self, DECLINATIONY) += step_declinationy;

    STATE(self, VIEWZOOM) = step_viewzoom;
    const float viewrotation = mod_arith(DEGREES(step_viewrotation) + 180.0, 360.0) - 180.0;
    STATE(self, VIEWROTATION) = viewrotation;

    { // Gridmap state update
        const float x = STATE(self, ACTUAL_X);
        const float y = STATE(self, ACTUAL_Y);
        const float scale = expf(SETTING(self, GRIDMAP_SCALE));
        const float scale_x = SETTING(self, GRIDMAP_SCALE_X);
        const float scale_y = SETTING(self, GRIDMAP_SCALE_Y);
        const float scaled_size = scale * GRID_SIZE;
        STATE(self, GRIDMAP_X) = mod_arith(fabsf(x * scale_x), scaled_size) / scaled_size * GRID_SIZE;
        STATE(self, GRIDMAP_Y) = mod_arith(fabsf(y * scale_y), scaled_size) / scaled_size * GRID_SIZE;
        if (x < 0.0) {
            STATE(self, GRIDMAP_X) = GRID_SIZE - STATE(self, GRIDMAP_X);
        }
        if (y < 0.0) {
            STATE(self, GRIDMAP_Y) = GRID_SIZE - STATE(self, GRIDMAP_Y);
        }
    }

    float base_radius = expf(BASEVAL(self, RADIUS_LOGARITHMIC));
    STATE(self, BARREL_ROTATION) += step_barrel_rotation;

    // FIXME: does happen (interpolation problem?)
    if (STATE(self, PRESSURE) <= 0.0) STATE(self, PRESSURE) = 0.0;
    const float pressure = STATE(self, PRESSURE);

    { // start / end stroke (for "stroke" input only)
      const float lim = 0.0001;
      const float threshold = BASEVAL(self, STROKE_THRESHOLD);
      const float started = STATE(self, STROKE_STARTED);
      if (!started && pressure > threshold + lim) {
        // start new stroke
        STATE(self, STROKE_STARTED) = 1;
        STATE(self, STROKE) = 0.0;
      } else if (started && pressure <= threshold * 0.9 + lim) {
        // end stroke
        STATE(self, STROKE_STARTED) = 0;
      }
    }

    // now follows input handling

    //adjust speed with viewzoom
    const float norm_dx = step_dx / step_dtime * STATE(self, VIEWZOOM);
    const float norm_dy = step_dy / step_dtime * STATE(self, VIEWZOOM);

    const float norm_speed = hypotf(norm_dx, norm_dy);
    //norm_dist should relate to brush size, whereas norm_speed should not
    const float norm_dist = hypotf(step_dx / step_dtime / base_radius, step_dy / step_dtime / base_radius) * step_dtime;

    float inputs[MYPAINT_BRUSH_INPUTS_COUNT];

    INPUT(PRESSURE) = pressure * expf(BASEVAL(self, PRESSURE_GAIN_LOG));

    const float m0 = self->speed_mapping_m[0];
    const float q0 = self->speed_mapping_q[0];
    const float m1 = self->speed_mapping_m[1];
    const float q1 = self->speed_mapping_q[1];
    INPUT(SPEED1) = log(self->speed_mapping_gamma[0] + STATE(self, NORM_SPEED1_SLOW)) * m0 + q0;
    INPUT(SPEED2) = log(self->speed_mapping_gamma[1] + STATE(self, NORM_SPEED2_SLOW)) * m1 + q1;

    INPUT(RANDOM) = self->random_input;
    INPUT(STROKE) = MIN(STATE(self, STROKE), 1.0);

    //correct direction for varying view rotation
    const float dir_angle = atan2f(STATE(self, DIRECTION_DY), STATE(self, DIRECTION_DX));
    INPUT(DIRECTION) = mod_arith(DEGREES(dir_angle) + viewrotation + 180.0, 180.0);
    const float dir_angle_360 = atan2f(STATE(self, DIRECTION_ANGLE_DY), STATE(self, DIRECTION_ANGLE_DX));
    INPUT(DIRECTION_ANGLE) = fmodf(DEGREES(dir_angle_360) + viewrotation + 360.0, 360.0) ;
    INPUT(TILT_DECLINATION) = STATE(self, DECLINATION);
    //correct ascension for varying view rotation, use custom mod
    INPUT(TILT_ASCENSION) = mod_arith(STATE(self, ASCENSION) + viewrotation + 180.0, 360.0) - 180.0;
    INPUT(VIEWZOOM) = BASEVAL(self, RADIUS_LOGARITHMIC) - logf(base_radius / STATE(self, VIEWZOOM));
    INPUT(ATTACK_ANGLE) = smallest_angular_difference(STATE(self, ASCENSION), mod_arith(DEGREES(dir_angle_360) + 90, 360));
    INPUT(BRUSH_RADIUS) = BASEVAL(self, RADIUS_LOGARITHMIC);

    INPUT(GRIDMAP_X) = CLAMP(STATE(self, GRIDMAP_X), 0.0, GRID_SIZE);
    INPUT(GRIDMAP_Y) = CLAMP(STATE(self, GRIDMAP_Y), 0.0, GRID_SIZE);

    INPUT(TILT_DECLINATIONX) = STATE(self, DECLINATIONX);
    INPUT(TILT_DECLINATIONY) = STATE(self, DECLINATIONY);

    INPUT(CUSTOM) = STATE(self, CUSTOM_INPUT);
    INPUT(BARREL_ROTATION) = mod_arith(STATE(self, BARREL_ROTATION), 360);

    if (self->print_inputs) {
        print_inputs(self, inputs);
    }

    for (int i = 0; i < MYPAINT_BRUSH_SETTINGS_COUNT; i++) {
      self->settings_value[i] = mypaint_mapping_calculate(self->settings[i], (inputs));
    }

    {
      const float fac = 1.0 - exp_decay(SETTING(self, SLOW_TRACKING_PER_DAB), step_ddab);
      STATE(self, ACTUAL_X) += (STATE(self, X) - STATE(self, ACTUAL_X)) * fac;
      STATE(self, ACTUAL_Y) += (STATE(self, Y) - STATE(self, ACTUAL_Y)) * fac;
    }

    { // slow speed
      const float fac1 = 1.0 - exp_decay(SETTING(self, SPEED1_SLOWNESS), step_dtime);
      STATE(self, NORM_SPEED1_SLOW) += (norm_speed - STATE(self, NORM_SPEED1_SLOW)) * fac1;
      const float fac2 = 1.0 - exp_decay (SETTING(self, SPEED2_SLOWNESS), step_dtime);
      STATE(self, NORM_SPEED2_SLOW) += (norm_speed - STATE(self, NORM_SPEED2_SLOW)) * fac2;
    }

    { // slow speed, but as vector this time
      float time_constant = expf(SETTING(self, OFFSET_BY_SPEED_SLOWNESS)*0.01)-1.0;
      // Workaround for a bug that happens mainly on Windows, causing
      // individual dabs to be placed far far away. Using the speed
      // with zero filtering is just asking for trouble anyway.
      if (time_constant < 0.002) time_constant = 0.002;
      const float fac = 1.0 - exp_decay (time_constant, step_dtime);
      STATE(self, NORM_DX_SLOW) += (norm_dx - STATE(self, NORM_DX_SLOW)) * fac;
      STATE(self, NORM_DY_SLOW) += (norm_dy - STATE(self, NORM_DY_SLOW)) * fac;
    }

    { // orientation (similar lowpass filter as above, but use dabtime instead of wallclock time)
      // adjust speed with viewzoom
      float dx = step_dx * STATE(self, VIEWZOOM);
      float dy = step_dy * STATE(self, VIEWZOOM);

      const float step_in_dabtime = hypotf(dx, dy);
      const float fac = 1.0 - exp_decay(expf(SETTING(self, DIRECTION_FILTER) * 0.5) - 1.0, step_in_dabtime);

      const float dx_old = STATE(self, DIRECTION_DX);
      const float dy_old = STATE(self, DIRECTION_DY);

      // 360 Direction
      STATE(self, DIRECTION_ANGLE_DX) += (dx - STATE(self, DIRECTION_ANGLE_DX)) * fac;
      STATE(self, DIRECTION_ANGLE_DY) += (dy - STATE(self, DIRECTION_ANGLE_DY)) * fac;

      // use the opposite speed vector if it is closer (we don't care about 180 degree turns)
      if (SQR(dx_old-dx) + SQR(dy_old-dy) > SQR(dx_old-(-dx)) + SQR(dy_old-(-dy))) {
        dx = -dx;
        dy = -dy;
      }
      STATE(self, DIRECTION_DX) += (dx - STATE(self, DIRECTION_DX)) * fac;
      STATE(self, DIRECTION_DY) += (dy - STATE(self, DIRECTION_DY)) * fac;
    }

    { // custom input
      const float fac = 1.0 - exp_decay (SETTING(self, CUSTOM_INPUT_SLOWNESS), 0.1);
      STATE(self, CUSTOM_INPUT) += (SETTING(self, CUSTOM_INPUT) - STATE(self, CUSTOM_INPUT)) * fac;
    }

    { // stroke length
      const float frequency = expf(-SETTING(self, STROKE_DURATION_LOGARITHMIC));
      const float stroke = MAX(0, STATE(self, STROKE) + norm_dist * frequency);
      const float wrap = 1.0 + MAX(0, SETTING(self, STROKE_HOLDTIME));
      // If the hold time is above 9.9, it is considered infinite, and if the stroke value has reached
      // that threshold it is no longer updated (until the stroke is reset, or the hold time changes).
      if (stroke >= wrap && wrap > 9.9 + 1.0) {
        STATE(self, STROKE) = 1.0;
      } else if (stroke >= wrap) {
        STATE(self, STROKE) = fmodf(stroke, wrap);
      } else {
        STATE(self, STROKE) = stroke;
      }
    }

    // calculate final radius
    const float radius_log = SETTING(self, RADIUS_LOGARITHMIC);
    STATE(self, ACTUAL_RADIUS) = expf(radius_log);
    if (STATE(self, ACTUAL_RADIUS) < ACTUAL_RADIUS_MIN) STATE(self, ACTUAL_RADIUS) = ACTUAL_RADIUS_MIN;
    if (STATE(self, ACTUAL_RADIUS) > ACTUAL_RADIUS_MAX) STATE(self, ACTUAL_RADIUS) = ACTUAL_RADIUS_MAX;

    // aspect ratio (needs to be calculated here because it can affect the dab spacing)
    STATE(self, ACTUAL_ELLIPTICAL_DAB_RATIO) = SETTING(self, ELLIPTICAL_DAB_RATIO);
    // correct dab angle for view rotation
    STATE(self, ACTUAL_ELLIPTICAL_DAB_ANGLE) = mod_arith(SETTING(self, ELLIPTICAL_DAB_ANGLE) - viewrotation + 180.0, 180.0) - 180.0;
  }

  float *fetch_smudge_bucket(MyPaintBrush *self) {
    if (!self->smudge_buckets || !self->num_buckets) {
      return &STATE(self, SMUDGE_RA);
    }
    const int bucket_index = CLAMP(roundf(SETTING(self, SMUDGE_BUCKET)), 0, self->num_buckets - 1);
    if (self->min_bucket_used == -1 || self->min_bucket_used > bucket_index) {
      self->min_bucket_used = bucket_index;
    }
    if (self->max_bucket_used < bucket_index) {
      self->max_bucket_used = bucket_index;
    }
    return &self->smudge_buckets[bucket_index * SMUDGE_BUCKET_SIZE];
  }

  gboolean
  update_smudge_color(
      const MyPaintBrush* self, MyPaintSurface* surface, float* const smudge_bucket, const float smudge_length, int px,
      int py, const float radius, const float legacy_smudge, const float paint_factor, gboolean legacy)
  {

      // Value between 0.01 and 1.0 that determines how often the canvas should be resampled
      float update_factor = MAX(0.01, smudge_length);


      // Calling get_color() is almost as expensive as rendering a
      // dab. Because of this we use the previous value if it is not
      // expected to hurt quality too much. We call it at most every
      // second dab.
      float r, g, b, a;
      const float smudge_length_log = SETTING(self, SMUDGE_LENGTH_LOG);

      const float recentness = smudge_bucket[PREV_COL_RECENTNESS] * update_factor;
      smudge_bucket[PREV_COL_RECENTNESS] = recentness;

      const float margin = 0.0000000000000001;
      if (recentness < MIN(1.0, powf(0.5 * update_factor, smudge_length_log) + margin)) {
          if (recentness == 0.0) {
              // first initialization of smudge color (initiate with color sampled from canvas)
              update_factor = 0.0;
          }
          smudge_bucket[PREV_COL_RECENTNESS] = 1.0;

          const float radius_log = SETTING(self, SMUDGE_RADIUS_LOG);
          const float smudge_radius = CLAMP(radius * expf(radius_log), ACTUAL_RADIUS_MIN, ACTUAL_RADIUS_MAX);

          // Sample colors on the canvas, using a negative value for the paint factor
          // means that the old sampling method is used, instead of weighted spectral.
          // Drawpile patch: allow bailing out early when sampling out of bounds.
          if (legacy) {
              if (!mypaint_surface_get_color(surface, px, py, smudge_radius, &r, &g, &b, &a)) {
                return FALSE;
              }
          } else if (!mypaint_surface2_get_color(
                  (MyPaintSurface2*)surface, px, py, smudge_radius, &r, &g, &b, &a, legacy_smudge ? -1.0 : paint_factor)) {
              return FALSE;
          }

          // don't draw unless the picked-up alpha is above a certain level
          // this is sort of like lock_alpha but for smudge
          // negative values reverse this idea
          const float smudge_op_lim = SETTING(self, SMUDGE_TRANSPARENCY);
          if ((smudge_op_lim > 0.0 && a < smudge_op_lim) || (smudge_op_lim < 0.0 && a > -smudge_op_lim)) {
              return TRUE; // signals the caller to return early
          }
          smudge_bucket[PREV_COL_R] = r;
          smudge_bucket[PREV_COL_G] = g;
          smudge_bucket[PREV_COL_B] = b;
          smudge_bucket[PREV_COL_A] = a;
      } else {
          r = smudge_bucket[PREV_COL_R];
          g = smudge_bucket[PREV_COL_G];
          b = smudge_bucket[PREV_COL_B];
          a = smudge_bucket[PREV_COL_A];
      }

      if (legacy_smudge) {
          const float fac_old = update_factor;
          const float fac_new = (1.0 - update_factor) * a;
          smudge_bucket[SMUDGE_R] = fac_old * smudge_bucket[SMUDGE_R] + fac_new * r;
          smudge_bucket[SMUDGE_G] = fac_old * smudge_bucket[SMUDGE_G] + fac_new * g;
          smudge_bucket[SMUDGE_B] = fac_old * smudge_bucket[SMUDGE_B] + fac_new * b;
          smudge_bucket[SMUDGE_A] = CLAMP((fac_old * smudge_bucket[SMUDGE_A] + fac_new), 0.0, 1.0);
      } else if (a > WGM_EPSILON * 10) {
          float prev_smudge_color[4] = {smudge_bucket[SMUDGE_R], smudge_bucket[SMUDGE_G], smudge_bucket[SMUDGE_B],
                                        smudge_bucket[SMUDGE_A]};
          float sampled_color[4] = {r, g, b, a};

          float* smudge_new = mix_colors(prev_smudge_color, sampled_color, update_factor, paint_factor);
          smudge_bucket[SMUDGE_R] = smudge_new[SMUDGE_R];
          smudge_bucket[SMUDGE_G] = smudge_new[SMUDGE_G];
          smudge_bucket[SMUDGE_B] = smudge_new[SMUDGE_B];
          smudge_bucket[SMUDGE_A] = smudge_new[SMUDGE_A];
      } else {
          // To avoid color noise from spectral mixing with a low alpha,
          // we'll just decrease the alpha of the existing smudge color.
          smudge_bucket[SMUDGE_A] = (smudge_bucket[SMUDGE_A] + a) / 2;
      }
      return FALSE; // signals the caller to not return early (the default)
  }

  float
  apply_smudge(
      const float* const smudge_bucket, const float smudge_value, const gboolean legacy_smudge,
      const float paint_factor, float* color_r, float* color_g, float* color_b)
  {
      float smudge_factor = MIN(1.0, smudge_value);

      // If the smudge color somewhat transparent, then the resulting
      // dab will do erasing towards that transparency level.
      // see also ../doc/smudge_math.png
      const float eraser_target_alpha =
          CLAMP((1.0 - smudge_factor) + smudge_factor * smudge_bucket[SMUDGE_A], 0.0, 1.0);

      if (eraser_target_alpha > 0) {
          if (legacy_smudge) {
              const float col_factor = 1.0 - smudge_factor;
              *color_r = (smudge_factor * smudge_bucket[SMUDGE_R] + col_factor * *color_r) / eraser_target_alpha;
              *color_g = (smudge_factor * smudge_bucket[SMUDGE_G] + col_factor * *color_g) / eraser_target_alpha;
              *color_b = (smudge_factor * smudge_bucket[SMUDGE_B] + col_factor * *color_b) / eraser_target_alpha;
          } else {
              float smudge_color[4] = {smudge_bucket[SMUDGE_R], smudge_bucket[SMUDGE_G], smudge_bucket[SMUDGE_B],
                                       smudge_bucket[SMUDGE_A]};
              float brush_color[4] = {*color_r, *color_g, *color_b, 1.0};
              float* color_new = mix_colors(smudge_color, brush_color, smudge_factor, paint_factor);
              *color_r = color_new[SMUDGE_R];
              *color_g = color_new[SMUDGE_G];
              *color_b = color_new[SMUDGE_B];
          }
      } else {
          // we are only erasing; the color does (should) not matter
          // we set the color to a clear red to see bugs easier.
          *color_r = 1.0;
          *color_g = 0.0;
          *color_b = 0.0;
      }
      return eraser_target_alpha;
  }

  // Called only from stroke_to(). Calculate everything needed to
  // draw the dab, then let the surface do the actual drawing.
  //
  // This is only gets called right after update_states_and_setting_values().
  // Returns TRUE if the surface was modified.
gboolean prepare_and_draw_dab (MyPaintBrush *self, MyPaintSurface * surface, gboolean legacy, gboolean linear)
  {
    const float opaque_fac = SETTING(self, OPAQUE_MULTIPLY);
    // ensure we don't get a positive result with two negative opaque values
    float opaque = MAX(0.0, SETTING(self, OPAQUE));
    opaque = CLAMP(opaque * opaque_fac, 0.0, 1.0);

    const float opaque_linearize = BASEVAL(self, OPAQUE_LINEARIZE);

    if (opaque_linearize) {
      // OPTIMIZE: no need to recalculate this for each dab
      float alpha, beta, alpha_dab, beta_dab;
      float dabs_per_pixel;
      // dabs_per_pixel is just estimated roughly, I didn't think hard
      // about the case when the radius changes during the stroke
      if (legacy) {
        dabs_per_pixel = BASEVAL(self, DABS_PER_ACTUAL_RADIUS) + BASEVAL(self, DABS_PER_BASIC_RADIUS);
      } else {
        dabs_per_pixel = STATE(self, DABS_PER_ACTUAL_RADIUS) + STATE(self, DABS_PER_BASIC_RADIUS);
      }
      dabs_per_pixel *= 2.0;

      // the correction is probably not wanted if the dabs don't overlap
      if (dabs_per_pixel < 1.0) dabs_per_pixel = 1.0;

      // interpret the user-setting smoothly
      dabs_per_pixel = 1.0 + opaque_linearize * (dabs_per_pixel-1.0);

      // see doc/brushdab_saturation.png
      //      beta = beta_dab^dabs_per_pixel
      // <==> beta_dab = beta^(1/dabs_per_pixel)
      alpha = opaque;
      beta = 1.0-alpha;
      beta_dab = powf(beta, 1.0/dabs_per_pixel);
      alpha_dab = 1.0-beta_dab;
      opaque = alpha_dab;
    }

    float x = STATE(self, ACTUAL_X);
    float y = STATE(self, ACTUAL_Y);

    float base_radius = expf(BASEVAL(self, RADIUS_LOGARITHMIC));

    // Directional offsets
    Offsets offs = directional_offsets(self, base_radius, (int)STATE(self, FLIP));
    x += offs.x;
    y += offs.y;

    const float view_zoom = STATE(self, VIEWZOOM);
    const float offset_by_speed = SETTING(self, OFFSET_BY_SPEED);
    if (offset_by_speed) {
        x += STATE(self, NORM_DX_SLOW) * offset_by_speed * 0.1 / view_zoom;
        y += STATE(self, NORM_DY_SLOW) * offset_by_speed * 0.1 / view_zoom;
    }

    const float offset_by_random = SETTING(self, OFFSET_BY_RANDOM);
    if (offset_by_random) {
        float amp = MAX(0.0, offset_by_random);
        x += rand_gauss (self->rng) * amp * base_radius;
        y += rand_gauss (self->rng) * amp * base_radius;
    }

    float radius = STATE(self, ACTUAL_RADIUS);
    const float radius_by_random = SETTING(self, RADIUS_BY_RANDOM);
    if (radius_by_random) {
        const float noise = rand_gauss(self->rng) * radius_by_random;
        float radius_log = SETTING(self, RADIUS_LOGARITHMIC) + noise;
        radius = CLAMP(expf(radius_log), ACTUAL_RADIUS_MIN, ACTUAL_RADIUS_MAX);
        float alpha_correction = SQR(STATE(self, ACTUAL_RADIUS) / radius);
        if (alpha_correction <= 1.0) {
            opaque *= alpha_correction;
        }
    }

    const float paint_factor = legacy ? 0.0 : SETTING(self, PAINT_MODE);
    const gboolean paint_setting_constant =
        legacy ? TRUE : mypaint_mapping_is_constant(self->settings[MYPAINT_BRUSH_SETTING_PAINT_MODE]);
    const gboolean legacy_smudge = paint_factor <= 0.0 && paint_setting_constant;

    //convert to RGB here instead of later
    // color part
    float color_h = BASEVAL(self, COLOR_H);
    float color_s = BASEVAL(self, COLOR_S);
    float color_v = BASEVAL(self, COLOR_V);
    hsv_to_rgb_float (&color_h, &color_s, &color_v);

    // update smudge color
    const float smudge_length = SETTING(self, SMUDGE_LENGTH);
    if (smudge_length < 1.0 && // default smudge length is 0.5, so the smudge factor is checked as well
        (SETTING(self, SMUDGE) != 0.0 || !mypaint_mapping_is_constant(self->settings[MYPAINT_BRUSH_SETTING_SMUDGE]))) {
        float* const bucket = fetch_smudge_bucket(self);
        gboolean return_early = update_smudge_color(
          self, surface, bucket, smudge_length, ROUND(x), ROUND(y), radius, legacy_smudge, paint_factor, legacy);
        if (return_early) {
          return FALSE;
        }
    }

    float eraser_target_alpha = 1.0;
    const float smudge_value = SETTING(self, SMUDGE);

    if (smudge_value > 0.0) {
      float* const bucket = fetch_smudge_bucket(self);
      eraser_target_alpha =
          apply_smudge(bucket, smudge_value, legacy_smudge, paint_factor, &color_h, &color_s, &color_v);
    }

    // eraser
    if (SETTING(self, ERASER)) {
      eraser_target_alpha *= (1.0-SETTING(self, ERASER));
    }

    /*
      If the colors are stored as linear sRGB, they need to be transformed to
      the form compatible with the hsv/hsl conversion functions, and then
      transformed back after the adjustments.
    */
    // Check whether the dynamics are used before the conditionals, to skip needless transformations.
    gboolean using_hsv_dynamics =
        SETTING(self, CHANGE_COLOR_H) || SETTING(self, CHANGE_COLOR_HSV_S) || SETTING(self, CHANGE_COLOR_V);
    gboolean using_hsl_dynamics = SETTING(self, CHANGE_COLOR_L) || SETTING(self, CHANGE_COLOR_HSL_S);
    gboolean using_color_dynamics = using_hsv_dynamics || using_hsl_dynamics;

    // delinearize
    if (linear && using_color_dynamics) {
      color_h = powf(color_h, 1 / 2.2);
      color_s = powf(color_s, 1 / 2.2);
      color_v = powf(color_v, 1 / 2.2);
    }

    // HSV color change
    if (using_hsv_dynamics) {
        rgb_to_hsv_float(&color_h, &color_s, &color_v);
        color_h += SETTING(self, CHANGE_COLOR_H);
        color_s += color_s * color_v * SETTING(self, CHANGE_COLOR_HSV_S);
        color_v += SETTING(self, CHANGE_COLOR_V);
        hsv_to_rgb_float(&color_h, &color_s, &color_v);
    }

    // HSL color change
    if (using_hsl_dynamics) {
      // (calculating way too much here, can be optimized if necessary)
      // this function will CLAMP the inputs
      rgb_to_hsl_float (&color_h, &color_s, &color_v);
      color_v += SETTING(self, CHANGE_COLOR_L);
      color_s += color_s * MIN(fabsf(1.0f - color_v), fabsf(color_v)) * 2.0f
        * SETTING(self, CHANGE_COLOR_HSL_S);
      hsl_to_rgb_float (&color_h, &color_s, &color_v);
    }

    // linearize
    if (linear && using_color_dynamics) {
      color_h = powf(color_h, 2.2);
      color_s = powf(color_s, 2.2);
      color_v = powf(color_v, 2.2);
    }

    float hardness = CLAMP(SETTING(self, HARDNESS), 0.0f, 1.0f);

    // anti-aliasing attempt (works surprisingly well for ink brushes)
    float current_fadeout_in_pixels = radius * (1.0 - hardness);
    float min_fadeout_in_pixels = SETTING(self, ANTI_ALIASING);
    if (current_fadeout_in_pixels < min_fadeout_in_pixels) {
      // need to soften the brush (decrease hardness), but keep optical radius
      // so we tune both radius and hardness, to get the desired fadeout_in_pixels
      float current_optical_radius = radius - (1.0 - hardness)*radius/2.0;

      // Equation 1: (new fadeout must be equal to min_fadeout)
      //   min_fadeout_in_pixels = radius_new*(1.0 - hardness_new)
      // Equation 2: (optical radius must remain unchanged)
      //   current_optical_radius = radius_new - (1.0-hardness_new)*radius_new/2.0
      //
      // Solved Equation 1 for hardness_new, using Equation 2: (thanks to mathomatic)
      float hardness_new = ((current_optical_radius - (min_fadeout_in_pixels/2.0))/(current_optical_radius + (min_fadeout_in_pixels/2.0)));
      // Using Equation 1:
      float radius_new = (min_fadeout_in_pixels/(1.0 - hardness_new));

      hardness = hardness_new;
      radius = radius_new;
    }

    // snap to pixel
    float snapToPixel = SETTING(self, SNAP_TO_PIXEL);
    if (snapToPixel > 0.0)
    {
      // linear interpolation between non-snapped and snapped
      const float snapped_x = floor(x) + 0.5;
      const float snapped_y = floor(y) + 0.5;
      x = x + (snapped_x - x) * snapToPixel;
      y = y + (snapped_y - y) * snapToPixel;

      float snapped_radius = roundf(radius * 2.0) / 2.0;
      if (snapped_radius < 0.5)
        snapped_radius = 0.5;

      if (snapToPixel > 0.9999 )
      {
        snapped_radius -= 0.0001; // this fixes precision issues where
                                  // neighbor pixels could be wrongly painted
      }

      radius = radius + (snapped_radius - radius) * snapToPixel;
    }

    const float dab_ratio = STATE(self, ACTUAL_ELLIPTICAL_DAB_RATIO);
    const float dab_angle = STATE(self, ACTUAL_ELLIPTICAL_DAB_ANGLE);
    const float lock_alpha = SETTING(self, LOCK_ALPHA);
    const float colorize = SETTING(self, COLORIZE);

    if (legacy) {
    return mypaint_surface_draw_dab (
        surface, x, y, radius, color_h, color_s, color_v, opaque, hardness, eraser_target_alpha,
        dab_ratio, dab_angle, lock_alpha, colorize);
    }
    else {
    const float posterize = SETTING(self, POSTERIZE);
    const float posterize_num = SETTING(self, POSTERIZE_NUM);
    return mypaint_surface2_draw_dab (
        (MyPaintSurface2*)surface, x, y, radius, color_h, color_s, color_v, opaque, hardness, eraser_target_alpha,
        dab_ratio, dab_angle, lock_alpha, colorize, posterize, posterize_num, paint_factor);
    }
  }

  // Drawpile patch: cap dabs counts to sane values to avoid chugging other users.

  static inline float base_dabs_per_second(MyPaintBrush *self)
  {
      const float value = BASEVAL(self, DABS_PER_SECOND);
      return MIN(value, 200.0f);
  }

  static inline float state_dabs_per_second(MyPaintBrush *self)
  {
      const float value = STATE(self, DABS_PER_SECOND);
      return MIN(value, 200.0f);
  }

  static inline float
  legacy_dab_count(float dist, float base_radius, float dt, MyPaintBrush* self)
  {
      const float dpar_base = BASEVAL(self, DABS_PER_ACTUAL_RADIUS);
      const float bounded_dpar_base = MIN(dpar_base, 20.0f);
      const float num_from_actual_radius = dist / STATE(self, ACTUAL_RADIUS) * bounded_dpar_base;
      const float dpbr_base = BASEVAL(self, DABS_PER_BASIC_RADIUS);
      const float bounded_dpbr_base = MIN(dpbr_base, 20.0f - MAX(bounded_dpar_base, 0.0f));
      const float num_from_basic_radius = dist / base_radius * bounded_dpbr_base;
      const float num_from_seconds = dt * base_dabs_per_second(self);
      return num_from_actual_radius + num_from_basic_radius + num_from_seconds;
  }

  static inline float
  state_based_dab_count(float dist, float base_radius, float dt, MyPaintBrush* self)
  {

      const float dpar_state = STATE(self, DABS_PER_ACTUAL_RADIUS);
      const float dpar = dpar_state && !isnan(dpar_state) ? dpar_state : BASEVAL(self, DABS_PER_ACTUAL_RADIUS);
      const float bounded_dpar = MIN(dpar, 20.0f);
      const float num_by_actual_radius = dist / STATE(self, ACTUAL_RADIUS) * bounded_dpar;

      const float dpbr_state = STATE(self, DABS_PER_BASIC_RADIUS);
      const float dpbr = dpbr_state && !isnan(dpbr_state) ? dpbr_state : BASEVAL(self, DABS_PER_BASIC_RADIUS);
      const float bounded_dpbr = MIN(dpbr, 20.0f - MAX(bounded_dpar, 0.0f));
      const float num_by_basic_radius = dist / base_radius * bounded_dpbr;

      const float dps_state = state_dabs_per_second(self);
      const float num_by_time_delta = dt * (!isnan(dps_state) ? dps_state : base_dabs_per_second(self));
      return num_by_actual_radius + num_by_basic_radius + num_by_time_delta;
  }

  // How many dabs will be drawn between the current and the next (x, y, +dt) position?
  float count_dabs_to (MyPaintBrush *self, float x, float y, float dt, gboolean legacy)
  {
    const float base_radius_log = BASEVAL(self, RADIUS_LOGARITHMIC);
    const float base_radius = CLAMP(expf(base_radius_log), ACTUAL_RADIUS_MIN, ACTUAL_RADIUS_MAX);

    if (STATE(self, ACTUAL_RADIUS) == 0.0) {
      STATE(self, ACTUAL_RADIUS) = base_radius;
    }

    const float dx = x - STATE(self, X);
    const float dy = y - STATE(self, Y);

    float dist;

    if (STATE(self, ACTUAL_ELLIPTICAL_DAB_RATIO) > 1.0) {
      // code duplication, see calculate_rr in mypaint-tiled-surface.c
      float angle_rad = RADIANS(STATE(self, ACTUAL_ELLIPTICAL_DAB_ANGLE));
      float cs = cos(angle_rad);
      float sn = sin(angle_rad);
      float yyr = (dy * cs - dx * sn) * STATE(self, ACTUAL_ELLIPTICAL_DAB_RATIO);
      float xxr = dy * sn + dx * cs;
      dist = sqrt(yyr * yyr + xxr * xxr);
    } else {
      dist = hypotf(dx, dy);
    }

    if (legacy) {
      return legacy_dab_count(dist, base_radius, dt, self);
    } else {
      return state_based_dab_count(dist, base_radius, dt, self);
    }
  }

int
mypaint_brush_stroke_to_internal(
    MyPaintBrush* self, MyPaintSurface* surface, float x, float y, float pressure, float xtilt, float ytilt,
    double dtime, float viewzoom, float viewrotation, float barrel_rotation, gboolean legacy, gboolean linear);


/**
 * mypaint_brush_stroke_to:
 * @dtime: Time since last motion event, in seconds.
 *
 * Should be called once for each motion event.
 *
 * Returns: non-0 if the stroke is finished or empty, else 0.
*/
int
mypaint_brush_stroke_to(
    MyPaintBrush* self, MyPaintSurface* surface, float x, float y, float pressure, float xtilt, float ytilt,
    double dtime)
{
  const float viewzoom = 1.0;
  const float viewrotation = 0.0;
  const float barrel_rotation = 0.0;
  return mypaint_brush_stroke_to_internal(
      self, surface, x, y, pressure, xtilt, ytilt, dtime, viewzoom, viewrotation, barrel_rotation, TRUE, FALSE);
}

/**
 * mypaint_brush_stroke_to_2:
 * @dtime: Time since last motion event, in seconds.
 * @viewzoom: Canvas zoom; 1.0 = 100% zoom. Zoom value v *must* be in range:
 * 0.0 < v < FLOAT_MAX (reasonable max is probably always below 100). Zoom value
 * cannot be 0!
 *
 * Should be called once for each motion event.
 *
 * Returns: non-0 if the stroke is finished or empty, else 0.
 */
int
mypaint_brush_stroke_to_2(
    MyPaintBrush* self, MyPaintSurface2* surface, float x, float y, float pressure, float xtilt, float ytilt,
    double dtime, float viewzoom, float viewrotation, float barrel_rotation)
{
    return mypaint_brush_stroke_to_internal(
      self, mypaint_surface2_to_surface(surface), x, y, pressure, xtilt, ytilt, dtime, viewzoom, viewrotation, barrel_rotation, FALSE, FALSE);
}

/**
 * mypaint_brush_stroke_to_2_linearsRGB:
 *
 * Same as mypaint_brush_stroke_to_2, but color _dynamics_ operate in linear sRGB,
 * i.e. settings that change the hue/value/lightness/saturation of the brush color.
 */
int
mypaint_brush_stroke_to_2_linearsRGB(
    MyPaintBrush* self, MyPaintSurface2* surface, float x, float y, float pressure, float xtilt, float ytilt,
    double dtime, float viewzoom, float viewrotation, float barrel_rotation)
{
    return mypaint_brush_stroke_to_internal(
      self, mypaint_surface2_to_surface(surface), x, y, pressure, xtilt, ytilt, dtime, viewzoom, viewrotation, barrel_rotation, FALSE, TRUE);
}

int
mypaint_brush_stroke_to_internal(
    MyPaintBrush* self, MyPaintSurface* surface, float x, float y, float pressure, float xtilt, float ytilt,
    double dtime, float viewzoom, float viewrotation, float barrel_rotation, gboolean legacy, gboolean linear)
{
    const float max_dtime = 5;

    float tilt_ascension = 0.0;
    float tilt_declination = 90.0;
    float tilt_declinationx = 90.0;
    float tilt_declinationy = 90.0;
    if (xtilt != 0 || ytilt != 0) {
      // shield us from insane tilt input
      xtilt = CLAMP(xtilt, -1.0, 1.0);
      ytilt = CLAMP(ytilt, -1.0, 1.0);
      assert(isfinite(xtilt) && isfinite(ytilt));

      tilt_ascension = DEGREES(atan2(-xtilt, ytilt));
      const float rad = hypot(xtilt, ytilt);
      tilt_declination = 90-(rad*60);
      tilt_declinationx = (xtilt * 60);
      tilt_declinationy = (ytilt * 60);

      assert(isfinite(tilt_ascension));
      assert(isfinite(tilt_declination));
      assert(isfinite(tilt_declinationx));
      assert(isfinite(tilt_declinationy));
    }

    if (pressure <= 0.0) pressure = 0.0;
    if (!isfinite(x) || !isfinite(y) ||
        (x > 1e10 || y > 1e10 || x < -1e10 || y < -1e10)) {
      // workaround attempt for https://gna.org/bugs/?14372
      printf("Warning: ignoring brush::stroke_to with insane inputs (x = %f, y = %f)\n", (double)x, (double)y);
      x = 0.0;
      y = 0.0;
      pressure = 0.0;
      viewzoom = 0.0;
      viewrotation = 0.0;
      barrel_rotation = 0.0;
    }
    // the assertion below is better than out-of-memory later at save time
    assert(x < 1e8 && y < 1e8 && x > -1e8 && y > -1e8);

    if (dtime < 0) printf("Time jumped backwards by dtime=%f seconds!\n", dtime);
    if (dtime <= 0) dtime = 0.0001; // protect against possible division by zero bugs

    if (dtime > 0.100 && pressure && STATE(self, PRESSURE) == 0) {
      // Workaround for tablets that don't report motion events without pressure.
      // This is to avoid linear interpolation of the pressure between two events.
      mypaint_brush_stroke_to_internal(
          self, surface, x, y, 0.0, 90.0, 0.0, dtime - 0.0001, viewzoom, viewrotation, barrel_rotation, legacy, linear);
      dtime = 0.0001;
    }

    // skip some length of input if requested (for stable tracking noise)
    if (self->skip > 0.001) {
      float dist = hypotf(self->skip_last_x-x, self->skip_last_y-y);
      self->skip_last_x = x;
      self->skip_last_y = y;
      self->skipped_dtime += dtime;
      self->skip -= dist;
      dtime = self->skipped_dtime;

      if (self->skip > 0.001 && !(dtime > max_dtime || self->reset_requested))
        return FALSE;

      // skipped
      self->skip = 0;
      self->skip_last_x = 0;
      self->skip_last_y = 0;
      self->skipped_dtime = 0;
    }

    { // calculate the actual "virtual" cursor position

      // noise first
      if (BASEVAL(self, TRACKING_NOISE)) {
        // OPTIMIZE: expf() called too often
        const float base_radius = expf(BASEVAL(self, RADIUS_LOGARITHMIC));
        const float noise = base_radius * BASEVAL(self, TRACKING_NOISE);

        if (noise > 0.001) {
          // we need to skip some length of input to make
          // tracking noise independent from input frequency
          self->skip = 0.5*noise;
          self->skip_last_x = x;
          self->skip_last_y = y;

          // add noise
          x += noise * rand_gauss(self->rng);
          y += noise * rand_gauss(self->rng);
        }
      }

      const float fac = 1.0 - exp_decay(BASEVAL(self, SLOW_TRACKING), 100.0 * dtime);
      x = STATE(self, X) + (x - STATE(self, X)) * fac;
      y = STATE(self, Y) + (y - STATE(self, Y)) * fac;
    }

    if (dtime > max_dtime || self->reset_requested) {
      self->reset_requested = FALSE;

      brush_reset(self);

      // reset value of random input
      self->random_input = rng_double_next(self->rng);

      STATE(self, X) = x;
      STATE(self, Y) = y;
      STATE(self, PRESSURE) = pressure;

      // not resetting, because they will get overwritten below:
      //dx, dy, dpress, dtime

      STATE(self, ACTUAL_X) = STATE(self, X);
      STATE(self, ACTUAL_Y) = STATE(self, Y);
      STATE(self, STROKE) = 1.0; // start in a state as if the stroke was long finished

      return TRUE;
    }

    enum { UNKNOWN, YES, NO } painted = UNKNOWN;
    double dtime_left = dtime;

    float step_ddab, step_dx, step_dy, step_dpressure, step_dtime;
    float step_declination, step_ascension, step_declinationx, step_declinationy, step_barrel_rotation;

    // draw many (or zero) dabs to the next position
    // see doc/images/stroke2dabs.png
    float dabs_moved = STATE(self, PARTIAL_DABS);
    float dabs_todo = count_dabs_to (self, x, y, dtime, legacy);
    while (dabs_moved + dabs_todo >= 1.0) { // there are dabs pending
      { // linear interpolation (nonlinear variant was too slow, see SVN log)
        float frac; // fraction of the remaining distance to move
        if (dabs_moved > 0) {
          // "move" the brush exactly to the first dab
          step_ddab = 1.0 - dabs_moved; // the step "moves" the brush by a fraction of one dab
          dabs_moved = 0;
        } else {
          step_ddab = 1.0; // the step "moves" the brush by exactly one dab
        }
        frac = step_ddab / dabs_todo;
        step_dx        = frac * (x - STATE(self, X));
        step_dy        = frac * (y - STATE(self, Y));
        step_dpressure = frac * (pressure - STATE(self, PRESSURE));
        step_dtime     = frac * (dtime_left - 0.0);
        // Though it looks different, time is interpolated exactly like x/y/pressure.
        step_declination = frac * (tilt_declination - STATE(self, DECLINATION));
        step_declinationx = frac * (tilt_declinationx - STATE(self, DECLINATIONX));
        step_declinationy = frac * (tilt_declinationy - STATE(self, DECLINATIONY));
        step_ascension   = frac * smallest_angular_difference(STATE(self, ASCENSION), tilt_ascension);
        //converts barrel_ration to degrees,
        step_barrel_rotation = frac * smallest_angular_difference(STATE(self, BARREL_ROTATION),barrel_rotation * 360);

        update_states_and_setting_values (self, step_ddab, step_dx, step_dy,
                                          step_dpressure, step_declination,
                                          step_ascension, step_dtime, viewzoom,
                                          viewrotation, step_declinationx,
                                          step_declinationy, step_barrel_rotation);
      }

      // Flips between 1 and -1, used for "mirrored" offsets.
      STATE(self, FLIP) *= -1;
      gboolean painted_now = prepare_and_draw_dab (self, surface, legacy, linear);
      if (painted_now) {
        painted = YES;
      } else if (painted == UNKNOWN) {
        painted = NO;
      }

      // update value of random input only when drawing the dab
      self->random_input = rng_double_next(self->rng);

      dtime_left -= step_dtime;
      dabs_todo = count_dabs_to(self, x, y, dtime_left, legacy);
    }

    {
      // "move" the brush to the current time (no more dab will happen)
      // Important to do this at least once every event, because
      // brush_count_dabs_to depends on the radius and the radius can
      // depend on something that changes much faster than just every
      // dab.
      step_ddab = dabs_todo; // the step "moves" the brush by a fraction of one dab
      step_dx        = x - STATE(self, X);
      step_dy        = y - STATE(self, Y);
      step_dpressure = pressure - STATE(self, PRESSURE);
      step_declination = tilt_declination - STATE(self, DECLINATION);
      step_declinationx = tilt_declinationx - STATE(self, DECLINATIONX);
      step_declinationy = tilt_declinationy - STATE(self, DECLINATIONY);
      step_ascension = smallest_angular_difference(STATE(self, ASCENSION), tilt_ascension);
      step_dtime     = dtime_left;
      step_barrel_rotation = smallest_angular_difference(STATE(self, BARREL_ROTATION), barrel_rotation * 360);

      update_states_and_setting_values (self, step_ddab, step_dx, step_dy, step_dpressure, step_declination, step_ascension, step_dtime, viewzoom, viewrotation, step_declinationx, step_declinationy, step_barrel_rotation);
    }

    // save the fraction of a dab that is already done now
    STATE(self, PARTIAL_DABS) = dabs_moved + dabs_todo;

    /* not working any more with the new rng...
    // next seed for the RNG (GRand has no get_state() and states[] must always contain our full state)
    STATE(self, RNG_SEED) = rng_double_next(self->rng);
    */

    // stroke separation logic (for undo/redo)

    if (painted == UNKNOWN) {
      if (self->stroke_current_idling_time > 0 || self->stroke_total_painting_time == 0) {
        // still idling
        painted = NO;
      } else {
        // probably still painting (we get more events than brushdabs)
        painted = YES;
        //if (pressure == 0) g_print ("info: assuming 'still painting' while there is no pressure\n");
      }
    }
    if (painted == YES) {
      //if (stroke_current_idling_time > 0) g_print ("idling ==> painting\n");
      self->stroke_total_painting_time += dtime;
      self->stroke_current_idling_time = 0;
      // force a stroke split after some time
      if (self->stroke_total_painting_time > 4 + 3*pressure) {
        // but only if pressure is not being released
        // FIXME: use some smoothed state for dpressure, not the output of the interpolation code
        //        (which might easily wrongly give dpressure == 0)
        if (step_dpressure >= 0) {
          return TRUE;
        }
      }
    } else if (painted == NO) {
      //if (stroke_current_idling_time == 0) g_print ("painting ==> idling\n");
      self->stroke_current_idling_time += dtime;
      if (self->stroke_total_painting_time == 0) {
        // not yet painted, start a new stroke if we have accumulated a lot of irrelevant motion events
        if (self->stroke_current_idling_time > 1.0) {
          return TRUE;
        }
      } else {
        // Usually we have pressure==0 here. But some brushes can paint
        // nothing at full pressure (eg gappy lines, or a stroke that
        // fades out). In either case this is the preferred moment to split.
        if (self->stroke_total_painting_time+self->stroke_current_idling_time > 0.9 + 5*pressure) {
          return TRUE;
        }
      }
    }
    return FALSE;
  }

#ifdef DRAWPILE_UNWANTED_MYPAINT_FEATURES
// Compat wrapper, for supporting libjson
static gboolean
obj_get(json_object *self, const gchar *key, json_object **obj_out) {
#if JSON_C_MINOR_VERSION >= 10
    return json_object_object_get_ex(self, key, obj_out)
        && (obj_out ? (*obj_out != NULL) : TRUE);
#else
    json_object *o = json_object_object_get(self, key);
    if (obj_out) {
        *obj_out = o;
    }
    return (o != NULL);
#endif
}

static gboolean
update_brush_setting_from_json_object(MyPaintBrush *self,
                                      char *setting_name,
                                      json_object *setting_obj)
{
    MyPaintBrushSetting setting_id = mypaint_brush_setting_from_cname(setting_name);

    if (setting_id >= MYPAINT_BRUSH_SETTINGS_COUNT) {
        fprintf(stderr, "Warning: Unknown setting_id: %d for setting: %s\n",
                setting_id, setting_name);
        return FALSE;
    }

    if (!json_object_is_type(setting_obj, json_type_object)) {
        fprintf(stderr, "Warning: Wrong type for setting: %s\n", setting_name);
        return FALSE;
    }

    // Base value
    json_object *base_value_obj = NULL;
    if (! obj_get(setting_obj, "base_value", &base_value_obj)) {
        fprintf(stderr, "Warning: No 'base_value' field for setting: %s\n", setting_name);
        return FALSE;
    }
    const double base_value = json_object_get_double(base_value_obj);
    mypaint_brush_set_base_value(self, setting_id, base_value);

    // Inputs
    json_object *inputs = NULL;
    if (! obj_get(setting_obj, "inputs", &inputs)) {
        fprintf(stderr, "Warning: No 'inputs' field for setting: %s\n", setting_name);
        return FALSE;
    }
    json_object_object_foreach(inputs, input_name, input_obj) {
        MyPaintBrushInput input_id = mypaint_brush_input_from_cname(input_name);

        if (!json_object_is_type(input_obj, json_type_array)) {
            fprintf(stderr, "Warning: Wrong inputs type for setting: %s\n", setting_name);
            return FALSE;
        }

        if (input_id >= MYPAINT_BRUSH_INPUTS_COUNT) {
            fprintf(stderr, "Warning: Unknown input_id: %d for input: %s\n",
                    input_id, input_name);
            return FALSE;
        }

        const int number_of_mapping_points = json_object_array_length(input_obj);

        mypaint_brush_set_mapping_n(self, setting_id, input_id, number_of_mapping_points);

        for (int i=0; i<number_of_mapping_points; i++) {
            json_object *mapping_point = json_object_array_get_idx(input_obj, i);

            json_object *x_obj = json_object_array_get_idx(mapping_point, 0);
            const float x = json_object_get_double(x_obj);
            json_object *y_obj = json_object_array_get_idx(mapping_point, 1);
            const float y = json_object_get_double(y_obj);

            mypaint_brush_set_mapping_point(self, setting_id, input_id, i, x, y);
        }
    }

    return TRUE;
}

static gboolean
update_brush_from_json_object(MyPaintBrush *self)
{
    // Check version
    json_object *version_object = NULL;
    if (! obj_get(self->brush_json, "version", &version_object)) {
        fprintf(stderr, "Error: No 'version' field for brush\n");
        return FALSE;
    }
    const int version = json_object_get_int(version_object);
    if (version != 3) {
        fprintf(stderr, "Error: Unsupported brush setting version: %d\n", version);
        return FALSE;
    }

    // Set settings
    json_object *settings = NULL;
    if (! obj_get(self->brush_json, "settings", &settings)) {
        fprintf(stderr, "Error: No 'settings' field for brush\n");
        return FALSE;
    }

    gboolean updated_any = FALSE;
    json_object_object_foreach(settings, setting_name, setting_obj) {
      updated_any |= update_brush_setting_from_json_object(self, setting_name, setting_obj);
    }
    return updated_any;
}

gboolean
mypaint_brush_from_string(MyPaintBrush *self, const char *string)
{
    json_object *brush_json = NULL;

    if (self->brush_json) {
        // Free
        json_object_put(self->brush_json);
        self->brush_json = NULL;
    }
    if (string) {
        brush_json = json_tokener_parse(string);
    }

    if (brush_json) {
        self->brush_json = brush_json;
        return update_brush_from_json_object(self);
    }
    else {
        self->brush_json = json_object_new_object();
        return FALSE;
    }
}
#endif

void
mypaint_brush_from_defaults(MyPaintBrush *self) {
    for (int s = 0; s < MYPAINT_BRUSH_SETTINGS_COUNT; s++) {
        for (int i = 0; i < MYPAINT_BRUSH_INPUTS_COUNT; i++) {
            mypaint_brush_set_mapping_n(self, s, i, 0);
        }

        const float def = mypaint_brush_setting_info(s)->def;
        mypaint_brush_set_base_value(self, s, def);
    }

    mypaint_brush_set_mapping_n(self, MYPAINT_BRUSH_SETTING_OPAQUE_MULTIPLY, MYPAINT_BRUSH_INPUT_PRESSURE, 2);
    mypaint_brush_set_mapping_point(self, MYPAINT_BRUSH_SETTING_OPAQUE_MULTIPLY, MYPAINT_BRUSH_INPUT_PRESSURE, 0, 0.0, 0.0);
    mypaint_brush_set_mapping_point(self, MYPAINT_BRUSH_SETTING_OPAQUE_MULTIPLY, MYPAINT_BRUSH_INPUT_PRESSURE, 1, 1.0, 1.0);
}

// Drawpile patch
RngDouble *
mypaint_brush_rng(MyPaintBrush *self)
{
    return self->rng;
}
