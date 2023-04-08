#ifndef MAPPING_H
#define MAPPING_H

#include "mypaint-config.h"
#include "mypaint-glib-compat.h"

G_BEGIN_DECLS

/*!
 * A sequence of piecewise linear functions
 *
 * Each function is built from a set of x,y control points.
 *
 * Primarily used to map inputs to setting values in instances of MyPaintBrush.
 *
 * @class MyPaintMapping
 */
typedef struct MyPaintMapping MyPaintMapping;

/*!
 * Create new mappings for a set number of inputs.
 *
 * @memberof MyPaintMapping
 */
MyPaintMapping * mypaint_mapping_new(int inputs_);

/*!
 * Free an instance of MyPaintMapping
 *
 * @memberof MyPaintMapping
 */
void mypaint_mapping_free(MyPaintMapping *self);

/*!
 * Get the base value of the mappings.
 *
 * @memberof MyPaintMapping
 */
float mypaint_mapping_get_base_value(MyPaintMapping *self);

/*!
 * Set the base value of the mappings.
 *
 * @memberof MyPaintMapping
 */
void mypaint_mapping_set_base_value(MyPaintMapping *self, float value);

/*!
 * Set the number of control points used for the input
 *
 * @memberof MyPaintMapping
 */
void mypaint_mapping_set_n (MyPaintMapping * self, int input, int n);

/*!
 * Get the number of control points used for the input
 *
 * @memberof MyPaintMapping
 */
int mypaint_mapping_get_n (MyPaintMapping * self, int input);

/*!
 * Set the x, y coordinates for a control point.
 *
 * @memberof MyPaintMapping
 */
void mypaint_mapping_set_point (MyPaintMapping * self, int input, int index, float x, float y);

/*!
 * Get the x, y coordinates for a control point.
 *
 * @memberof MyPaintMapping
 */
void mypaint_mapping_get_point (MyPaintMapping * self, int input, int index, float *x, float *y);

/*!
 * Returns TRUE if none of the mappings are used.
 *
 * @memberof MyPaintMapping
 *
 * @sa mypaint_brush_is_constant
 */
gboolean mypaint_mapping_is_constant(MyPaintMapping * self);

/*!
 * Get the number of inputs that have control points.
 *
 * @memberof MyPaintMapping
 */
int mypaint_mapping_get_inputs_used_n(MyPaintMapping *self);

/*!
 * Calculate the output of the mapping, given an input value for each mapping.
 *
 * @memberof MyPaintMapping
 */
float mypaint_mapping_calculate (MyPaintMapping * self, float * data);

/*!
 * Calculate the output for a single-input mapping.
 *
 * @memberof MyPaintMapping
 */
float mypaint_mapping_calculate_single_input (MyPaintMapping * self, float input);


G_END_DECLS

#endif // MAPPING_H
