/* libmypaint - The MyPaint Brush Library
 * Copyright (C) 2007-2008 Martin Renold <martinxyz@gmx.ch>
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

#ifndef MAPPING_C
#define MAPPING_C

#include "config.h"

#include <stdlib.h>
#include <assert.h>

#if MYPAINT_CONFIG_USE_GLIB
#include <glib.h>
#endif

#include "mypaint-mapping.h"

#include "helpers.h"

// user-defined mappings
// (the curves you can edit in the brush settings)

typedef struct {
  // a set of control points (stepwise linear)
  float xvalues[64];
  float yvalues[64];
  int n;
} ControlPoints;

struct MyPaintMapping {
    float base_value; // FIXME: accessed directly from mypaint-brush.c

    int inputs;
    ControlPoints * pointsList; // one for each input
    int inputs_used; // optimization

};


MyPaintMapping *
mypaint_mapping_new(int inputs_)
{
    MyPaintMapping *self = (MyPaintMapping *)malloc(sizeof(MyPaintMapping));

    self->inputs = inputs_;
    self->pointsList = (ControlPoints *)malloc(sizeof(ControlPoints)*self->inputs);
    int i = 0;
    for (i=0; i<self->inputs; i++) self->pointsList[i].n = 0;

    self->inputs_used = 0;
    self->base_value = 0;

    return self;
}

void
mypaint_mapping_free(MyPaintMapping *self)
{
    free(self->pointsList);
    free(self);
}

float mypaint_mapping_get_base_value(MyPaintMapping *self)
{
    return self->base_value;
}

void mypaint_mapping_set_base_value(MyPaintMapping *self, float value)
{
    self->base_value = value;
}

void mypaint_mapping_set_n (MyPaintMapping * self, int input, int n)
{
    assert (input >= 0 && input < self->inputs);
    assert (n >= 0 && n <= 64);
    assert (n != 1); // cannot build a linear mapping with only one point
    ControlPoints * p = self->pointsList + input;

    if (n != 0 && p->n == 0) self->inputs_used++;
    if (n == 0 && p->n != 0) self->inputs_used--;
    assert(self->inputs_used >= 0);
    assert(self->inputs_used <= self->inputs);

    p->n = n;
}


int mypaint_mapping_get_n (MyPaintMapping * self, int input)
{
    assert (input >= 0 && input < self->inputs);
    ControlPoints * p = self->pointsList + input;
    return p->n;
}

void mypaint_mapping_set_point (MyPaintMapping * self, int input, int index, float x, float y)
{
    assert (input >= 0 && input < self->inputs);
    assert (index >= 0 && index < 64);
    ControlPoints * p = self->pointsList + input;
    assert (index < p->n);

    if (index > 0) {
      assert (x >= p->xvalues[index-1]);
    }

    p->xvalues[index] = x;
    p->yvalues[index] = y;
}

void mypaint_mapping_get_point (MyPaintMapping * self, int input, int index, float *x, float *y)
{
    assert (input >= 0 && input < self->inputs);
    assert (index >= 0 && index < 64);
    ControlPoints * p = self->pointsList + input;
    assert (index < p->n);

    *x = p->xvalues[index];
    *y = p->yvalues[index];
}

gboolean mypaint_mapping_is_constant(MyPaintMapping * self)
{
    return self->inputs_used == 0;
}

int
mypaint_mapping_get_inputs_used_n(MyPaintMapping *self)
{
    return self->inputs_used;
}

float mypaint_mapping_calculate (MyPaintMapping * self, float * data)
{
    int j;
    float result;
    result = self->base_value;

    // constant mapping (common case)
    if (self->inputs_used == 0) return result;

    for (j=0; j<self->inputs; j++) {
      ControlPoints * p = self->pointsList + j;

      if (p->n) {
        float x, y;
        x = data[j];

        // find the segment with the slope that we need to use
        float x0, y0, x1, y1;
        x0 = p->xvalues[0];
        y0 = p->yvalues[0];
        x1 = p->xvalues[1];
        y1 = p->yvalues[1];

        int i;
        for (i=2; i<p->n && x>x1; i++) {
          x0 = x1;
          y0 = y1;
          x1 = p->xvalues[i];
          y1 = p->yvalues[i];
        }

        if (x0 == x1 || y0 == y1) {
          y = y0;
        } else {
          // linear interpolation
          y = (y1*(x - x0) + y0*(x1 - x)) / (x1 - x0);
        }

        result += y;
      }
    }
    return result;
}

// used in mypaint itself for the global pressure mapping
float mypaint_mapping_calculate_single_input (MyPaintMapping * self, float input)
{
    assert(self->inputs == 1);
    return mypaint_mapping_calculate(self, &input);
}
#endif //MAPPING_C
