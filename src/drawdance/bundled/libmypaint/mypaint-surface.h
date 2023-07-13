#ifndef MYPAINTSURFACE_H
#define MYPAINTSURFACE_H

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
#include "mypaint-rectangle.h"

G_BEGIN_DECLS

typedef struct MyPaintSurface MyPaintSurface;

/*!
 * Function used to retrieve the average color/alpha from a region of a surface.
 *
 * This kind of function calculates the average color/alpha from the pixels
 * of the given surface, selected and weighted respectively by the area
 * and opacity of a uniform dab with a given center and radius.
 *
 * @param self The surface to fetch the pixels from
 * @param x, y The center of the dab that determine which pixels
 * to include, and their respective impact on the result.
 * @param radius The radius of the dab.
 * @param[out] color_r, color_g, color_b, color_a
 * The destination of the resulting colors, range: [0.0, 1.0]
 */
typedef void (*MyPaintSurfaceGetColorFunction) (
  MyPaintSurface *self,
  float x, float y,
  float radius,
  float * color_r, float * color_g, float * color_b, float * color_a
  );

/*!
 * Function used to draw a dab with the given properties on the surface.
 *
 * The function drawing the dab is one of the core parts of any surface
 * implementation. These functions will usually not be invoked directly
 * by library users, but indirectly as part of the ::mypaint_brush_stroke_to
 * call, where the interpolated parameters for each dab are calculated.
 *
 * @param self The surface on which the dab will be drawn
 * @param x, y The center of the dab
 * @param radius The radius of the dab
 * @param color_r, color_g, color_b, opaque color/opacity of the dab
 * @param hardness Determines how opacity is retained for
 * pixels further from the dab center.
 * @param alpha_eraser Hell if I know
 * @param aspect_ratio The width/height ratio of the dab.
 * @param angle The angle of the dab (applied after __aspect_ratio__).
 * @param lock_alpha The extent to which the alpha values of affected pixels
 * on the surface are retained, regardless of other dab parameters.
 * @param colorize: The extent to which the dab will __only__ affect the hue of
 * existing pixels on the surface.
 *
 * @memberof MyPaintSurface
 */
typedef int (*MyPaintSurfaceDrawDabFunction) (
  MyPaintSurface *self,
  float x, float y,
  float radius,
  float color_r, float color_g, float color_b,
  float opaque, float hardness,
  float alpha_eraser,
  float aspect_ratio, float angle,
  float lock_alpha,
  float colorize
  );

/*!
 * Destructor for surface implementations.
 * @param self The surface to free/destroy
 *
 * @memberof MyPaintSurface
 */
typedef void (*MyPaintSurfaceDestroyFunction) (MyPaintSurface *self);

/*!
 * Function for rendering a png file from a surface.
 * @param self The surface to render from
 * @param path The destination of the png file
 * @param x, y The top left corner of the area to export
 * @param width, height The dimensions of the area & of the resulting png
 *
 * @memberof MyPaintSurface
 */
typedef void (*MyPaintSurfaceSavePngFunction) (MyPaintSurface *self, const char *path, int x, int y, int width, int height);

/*!
 * Prepare the surface for an atomic set of stroke operations.
 *
 * Each call to functions of this type should be matched by a call to a
 * ::MyPaintSurfaceEndAtomicFunction with the same surface.
 *
 * @memberof MyPaintSurface
 */
typedef void (*MyPaintSurfaceBeginAtomicFunction) (MyPaintSurface *self);

/*!
 * Finalize an atomic set of stroke operations, setting an invalidation rectangle.
 *
 * Each call to functions of this type should be matched by a call to a
 * ::MyPaintSurfaceBeginAtomicFunction with the same surface.
 *
 * @memberof MyPaintSurface
 */
typedef void (*MyPaintSurfaceEndAtomicFunction) (MyPaintSurface *self, MyPaintRectangle *roi);

/*!
  * Abstract surface type for the MyPaint brush engine. The surface interface
  * lets the brush engine specify dabs to render, and get color data for use
  * in interpolations.
  *
  * @sa MyPaintSurface2
  */
struct MyPaintSurface {
    /*! Function for drawing a dab on the surface.
     *
     * See ::MyPaintSurfaceDrawDabFunction for details.
     */
    MyPaintSurfaceDrawDabFunction draw_dab;
    /*!
     * Function for retrieving color data from the surface.
     *
     * See ::MyPaintSurfaceGetColorFunction for details.
     */
    MyPaintSurfaceGetColorFunction get_color;
    /*!
     * Prepare the surface for a set of atomic stroke/dab operations.
     *
     * See ::MyPaintSurfaceBeginAtomicFunction for details.
     */
    MyPaintSurfaceBeginAtomicFunction begin_atomic;
    /*!
     * Finalize the operations run after a call to #begin_atomic.
     *
     * See ::MyPaintSurfaceEndAtomicFunction for details.
     */
    MyPaintSurfaceEndAtomicFunction end_atomic;
    /*!
    * Destroy the surface (free up all allocated resources).
    */
    MyPaintSurfaceDestroyFunction destroy;
    /*! Save a region of the surface to a png file.
     *
     * See MyPaintSurfaceSavePngFunction for details
     */
    MyPaintSurfaceSavePngFunction save_png;
    /*! Reference count - number of references to the struct
     *
     * This is only useful when used in a context with automatic
     * memory management and can be ignored if construction/destruction
     * is handled manually.
     */
    int refcount;
};

/*!
 * Invoke MyPaintSurface::draw_dab
 *
 * @memberof MyPaintSurface
 * @sa MyPaintSurfaceDrawDabFunction
 */
int mypaint_surface_draw_dab(
    MyPaintSurface* self, float x, float y, float radius, float color_r, float color_g, float color_b, float opaque,
    float hardness, float alpha_eraser, float aspect_ratio, float angle, float lock_alpha, float colorize);

/*!
 * Invoke MyPaintSurface::get_color
 *
 * @memberof MyPaintSurface
 * @sa MyPaintSurfaceGetColorFunction
 */
void mypaint_surface_get_color(
    MyPaintSurface* self, float x, float y, float radius, float* color_r, float* color_g, float* color_b,
    float* color_a);

/*!
 * Invoke MyPaintSurface::get_color and return the alpha component
 *
 * @memberof MyPaintSurface
 * @sa MyPaintSurfaceGetColorFunction
 */
float mypaint_surface_get_alpha(MyPaintSurface* self, float x, float y, float radius);

/*!
 * Invoke MyPaintSurface::save_png
 *
 * @memberof MyPaintSurface
 * @sa MyPaintSurfaceSavePngFunction
 */
void
mypaint_surface_save_png(MyPaintSurface *self, const char *path, int x, int y, int width, int height);


/*!
 * Invoke MyPaintSurface::begin_atomic
 *
 * @memberof MyPaintSurface
 * @sa MyPaintSurfaceBeginAtomicFunction
 */
void mypaint_surface_begin_atomic(MyPaintSurface *self);

/*!
 * Invoke MyPaintSurface::begin_atomic
 *
 * @memberof MyPaintSurface
 * @sa MyPaintSurfaceBeginAtomicFunction
 */
void mypaint_surface_end_atomic(MyPaintSurface *self, MyPaintRectangle *roi);

/*!
 * Set #refcount to 1
 *
 * @memberof MyPaintSurface
 */
void mypaint_surface_init(MyPaintSurface *self);

/*!
 * Increase #refcount by 1
 *
 * @memberof MyPaintSurface
 */

void mypaint_surface_ref(MyPaintSurface *self);

/*!
 * Decrease #refcount by 1 and call #destroy if it reaches 0
 *
 * @memberof MyPaintSurface
 */
void mypaint_surface_unref(MyPaintSurface *self);


/* Extended interface */

typedef struct MyPaintSurface2 MyPaintSurface2;

/*!
 * Like #MyPaintSurfaceDrawDabFunction, but supporting posterization and spectral mixing
 *
 * @param posterize Posterization factor
 * @param posterize_num Number of posterization levels
 * @param paint Spectral mixing factor
 *
 * @sa MyPaintSurfaceDrawDabFunction
 * @memberof MyPaintSurface2
 */
typedef int (*MyPaintSurfaceDrawDabFunction2) (
  MyPaintSurface2 *self,
  float x, float y,
  float radius,
  float color_r, float color_g, float color_b,
  float opaque, float hardness,
  float alpha_eraser,
  float aspect_ratio, float angle,
  float lock_alpha,
  float colorize,
  float posterize,
  float posterize_num,
  float paint);


/*!
 * Like #MyPaintSurfaceGetColorFunction, but for spectral colors.
 *
 * @param paint Spectral mixing factor.
 * To what extent spectral weighting will be used in place of straight averaging.
 * Input value range: [0.0, 1.0]
 *
 * @sa MyPaintSurfaceGetColorFunction
 * @memberof MyPaintSurface2
 */
typedef void (*MyPaintSurfaceGetColorFunction2) (
  MyPaintSurface2 *self,
  float x, float y,
  float radius,
  float * color_r, float * color_g, float * color_b, float * color_a,
  float paint
  );

/*!
 * Like #MyPaintSurfaceEndAtomicFunction, but with support for multiple invalidation rectangles.
 *
 * @sa MyPaintSurfaceEndAtomicFunction
 * @memberof MyPaintSurface2
 */
typedef void (*MyPaintSurfaceEndAtomicFunction2) (MyPaintSurface2 *self, MyPaintRectangles *roi);

/*!
 * Extends MyPaintSurface with support for spectral ops and multiple bounding boxes.
 *
 * This extends the regular MyPaintSurface with three new
 * functions that support the use of spectral blending,
 * for drawing dabs and getting color, and additionally
 * supports using multiple invalidation rectangles when
 * finalizing drawing operations.
 *
 * The interface functions for MyPaintSurface can be called
 * with instances of MyPaintSurface2 via the use of
 * mypaint_surface2_to_surface (or just casting). Concrete
 * implementations of MypaintSurface2 should support this.
 *
 * @sa MyPaintSurface
 */
struct MyPaintSurface2 {
    /*! Parent interface */
    MyPaintSurface parent;
    /*! See #MyPaintSurfaceDrawDabFunction2 */
    MyPaintSurfaceDrawDabFunction2 draw_dab_pigment;
    /*! See #MyPaintSurfaceGetColorFunction2 */
    MyPaintSurfaceGetColorFunction2 get_color_pigment;
    /*! See #MyPaintSurfaceEndAtomicFunction2 */
    MyPaintSurfaceEndAtomicFunction2 end_atomic_multi;
};

/*!
 * Safely access the parent MyPaintSurface interface
 *
 * @memberof MyPaintSurface2
 */
MyPaintSurface* mypaint_surface2_to_surface(MyPaintSurface2 *self);

/*!
 * Call the surface's #get_color_pigment function.
 *
 * See #MyPaintSurfaceGetColorFunction2 for details.
 *
 * @memberof MyPaintSurface2
 */
void mypaint_surface2_get_color(
    MyPaintSurface2* self, float x, float y, float radius,float* color_r, float* color_g, float* color_b,
    float* color_a, float paint);

/*!
 * Call the surface's #end_atomic_multi function.
 *
 * See #MyPaintSurfaceEndAtomicFunction2 for details.
 *
 * @memberof MyPaintSurface2
 */
void mypaint_surface2_end_atomic(MyPaintSurface2 *self, MyPaintRectangles *roi);

/*!
 * Call the surface's #draw_dab_pigment function.
 *
 * See #MyPaintSurfaceDrawDabFunction2 for details.
 *
 * @memberof MyPaintSurface2
 */
int mypaint_surface2_draw_dab(
    MyPaintSurface2* self, float x, float y, float radius, float color_r, float color_g, float color_b, float opaque,
    float hardness, float alpha_eraser, float aspect_ratio, float angle, float lock_alpha, float colorize,
    float posterize, float posterize_num, float paint);

G_END_DECLS

#endif // MYPAINTSURFACE_H

