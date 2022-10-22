#ifndef MYPAINT_H
#define MYPAINT_H

/*!
 * \mainpage libmypaint - the MyPaint brush library
 *
 * \section Overview
 *
 * `libmypaint`, a.k.a. "the brush library" or "brushlib", provides a set of
 * structures and interfaces allowing applications to use MyPaint brushes to
 * draw strokes on surfaces.
 *
 * See the data structures list for a context-free overview of the functionality
 * provided. The most relevant structures (with associated functions) are:
 *
 * ---
 *
 * MyPaintSurface, MyPaintBrush, and MyPaintTiledSurface
 *
 * ---
 *
 * MyPaintSurface is an abstract interface providing the essential calls used
 * by the _brush engine_, which is made up of the MyPaintBrush struct and its
 * associated functions (or methods, if you like).
 *
 * Having MyPaintSurface be abstract does in theory allow users of the library
 * to provide their own surface implementations, but in order to be visually
 * conformant with MyPaint, they need to use the same (or very similar)
 * dab drawing and blending routines that are used by MyPaintTiledSurface,
 * requiring them to either derive their own surface from MyPaintTiledSurface,
 * or copy the relevant internal functions from the libmypaint source code.
 *
 * \subsection qwe Brief History
 * *
 * What is now `libmypaint` started out as
 * a part of the [MyPaint](https://github.com/mypaint/mypaint/) drawing program.
 * Over time the code was gradually separated from the rest of the application,
 * and eventually split out into a separate library. This evolution from an origin
 * in application-specific code, as opposed to a design intended to be used as an
 * independent library, is the reason why a lot of the code is structured the way
 * it is.
 *
 * @note
 * The focus of this timeline is development that relates to the parts
 * of MyPaint development that ultimately became libmypaint. It is only
 * a small fraction of the work that was undertaken over the covered time period.
 *
 * - __2004__:
 * Martin Renold published the first iteration of MyPaint,
 * a gtk-based minimal painting program.
 * At first, the only adjustable brush parameters were radius,
 * color (RGB) and opacity (erroneously named "opaque", which
 * remains the canonical name to this day).
 * Soon after, spacing and distance parameters were added as well.
 *
 * - __2005__:
 * First implementation of the infinite surface, parts
 * of which eventually turned into MyPaintTiledSurface.
 * The brush code was reworked, turning the color into setting values
 * (and switching those values from RGB to HSV). Apart from the color
 * components, 20 new settings were implemented (most of which remain today).
 *
 * - __2006__:
 * The `custom` setting & input were added, as well as `speed_log`, `speed_sqrt`
 * and `opaque_linearize`. The `color_saturation` setting was removed.
 *
 * - __2007__:
 * Color transformation settings were added, as well as
 * `smudge_length` and `tracking_noise`.
 *
 * - __2008__:
 * The `eraser` setting was added. At the end of 2008, the core drawing code
 * was split off into the "brushlib". Moved a lot of code from C to C++.
 *
 * - __2009__:
 * Dab angle and ratio settings were added, as well as direction filter.
 *
 * - __2010__:
 * The `smudge_radius_log` setting was added, as well
 * as the `tilt_declination` and `tilt_ascension` inputs.
 *
 * - __2011__:
 * The `lock_alpha` and `colorize` settings were added.
 *
 * - __2012__:
 * Initial split to `libmypaint`, with general dependency cleanup.
 * The brushlib was converted (back) to C, from C++.
 *
 * - __2013-2015__:
 * A lot of maintenance work and general improvements, preparing for
 * independent releases and usage of the library.
 *
 * - __2016__:
 * First release of libmypaint, which for unspecified reasons was
 * assigned the version `1.3.0`.
 * @note The version was almost certainly chosen because of the next minor
 * version of MyPaint, which was assumed to be the first release using
 * libmypaint as an external dependency.
 *
 * - __2017-2019__:
 * Gridmap settings and inputs were added, as well as many kinds of dab offsets.
 * Spectral blending was introduced, as well as additional stroke parameters that
 * let the engine be aware of canvas rotation and zoom when calculating angles
 * and speed values.
 * <br/><br/>
 * libmypaint `1.4.0` was released, not including the new features.
 *
 *
 * - __2020__:
 * The new features were backported in an API-compatible way, and `1.5.0` was released.
 *
 * \section Examples
 *
 * There is a minimal example in the `examples` folder in the source root, but
 * for now the best way to learn how the structures and functions can be used
 * is to look at the MyPaint source code. Searching for the string `mypaint_`
 * in the C/C++ files in the MyPaint source will yield all calls to libmypaint,
 * providing useful entry points for further exploration.
 */

/*!
 * If libmypaint is built with OpenMP support, this call ensures
 * that no more than #MYPAINT_MAX_THREADS will be used. If libmypaint
 * is _not_ built with OpenMP support, calling this is a no-op.
 */
void mypaint_init(void);

#endif // MYPAINT_H
