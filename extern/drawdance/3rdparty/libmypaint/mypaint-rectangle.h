#ifndef MYPAINTRECTANGLE_H
#define MYPAINTRECTANGLE_H

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
#include "mypaint-glib-compat.h"

G_BEGIN_DECLS

/*!
 * Representation of a rectangle, integer values: (x, y, w, h)
 *
 * Rectangles are only used for registering areas that have been affected
 * by brush strokes - the units of the parameters are pixels.
 *
 */
typedef struct {
    /*! The x-coordinate of the rectangle's top-left corner */
    int x;
    /*! The x-coordinate of the rectangle's top-left corner */
    int y;
    /*! The width of the rectangle */
    int width;
    /*! The height of the rectangle */
    int height;
} MyPaintRectangle;

/*!
 * Holds the size and location of a MyPaintRectangle array.
 *
 * Only used in the MyPaintSurface2 interface for handling multiple
 * areas affected by a stroke (only _actually_ used in
 * MyPaintTiledSurface2, which implements MyPaintSurface2).
 */
typedef struct {
  /*!
   * The number of rectangles
   *
   * The number of rectangles that the space pointed
   * to by MyPaintRectangles::rectangles can fit.
   */
  int num_rectangles;
  /*!
   * Pointer to a single, or an array of, MyPaintRectangle%s
   *
   * The space pointed to should fit at least n MyPaintRectangle%s,
   * where n = MyPaintRectangles::num_rectangles
   */
  MyPaintRectangle* rectangles;
} MyPaintRectangles;

/*!
 * Minimally expand a rectangle so that it contains a given point (x, y).
 *
 * @param[out] r The rectangle that may be expanded
 * @param x, y The coordinates of the point to expand to
 *
 * If the point (__x__, __y__) lies outside of the given rectangle
 * pointed to by  __r__, the rectangle is expanded the minimum amount
 * needed for the point to lie within its new boundaries.
 *
 * @memberof MyPaintRectangle
 */
void mypaint_rectangle_expand_to_include_point(MyPaintRectangle *r, int x, int y);

/*!
 * Minimally expand a rectangle so that it fully contains another
 *
 * @param[out] r The rectangle that may be expanded
 * @param other The rectangle to expand __r__ to
 *
 * If __r__ does not completely contain __other__, __r__ is expanded the minimum
 * amount needed for __other__ to lie fully within __r__'s boundaries.
 *
 * @memberof MyPaintRectangle
 */
void mypaint_rectangle_expand_to_include_rect(MyPaintRectangle *r, MyPaintRectangle *other);

/*!
 * Create a copy of the given rectangle.
 *
 * Allocates a new MyPaintRectangle, copies the input rectangle
 * to it, and returns its address.
 * If the allocation fails, __NULL__ is returned.
 *
 * @param r A pointer to the rectangle to copy.
 * @returns A pointer to a new copy of __r__, or __NULL__.
 *
 * @memberof MyPaintRectangle
 */
MyPaintRectangle *
mypaint_rectangle_copy(MyPaintRectangle *r);

G_END_DECLS

#endif // MYPAINTRECTANGLE_H
