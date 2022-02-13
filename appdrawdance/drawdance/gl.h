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
#ifndef DRAWDANCE_GL_H
#define DRAWDANCE_GL_H
#include <dpcommon/common.h>


/*
 * Macros for OpenGL error checking, which has a rather annoying interface. In
 * a release build, you probably want to disable this.
 */
#ifdef DP_GL_CHECKS
/*
 * Clear any piled up errors and warn about each one. Every function that does
 * error checking should make sure to clear errors beforehand, otherwise those
 * leftover errors can cause it to warn through no fault of its own. If you get
 * these warnings, it should mean you (or something else) did an unchecked call
 * somewhere, which failed but nobody collected its reported error.
 */
#    define DP_GL_CLEAR_ERROR()                           \
        do {                                              \
            unsigned int glerror;                         \
            while ((glerror = DP_gl_get_error()) != 0) {  \
                DP_gl_warn(glerror, "DP_GL_CLEAR_ERROR"); \
            }                                             \
        } while (0)
/*
 * Check if there was an OpenGL error and if so call DP_panic() to abort
 * execution. Used by the DP_GL and DP_GL_0 macros below.
 */
#    define DP_GL_CHECK_ERROR(FUNC)                   \
        do {                                          \
            unsigned int glerror;                     \
            if ((glerror = DP_gl_get_error()) != 0) { \
                DP_gl_panic(glerror, FUNC);           \
            }                                         \
        } while (0)
/*
 * Run the given OpenGL function and check for errors. So for example, instead
 * of `glActiveTexture(GL_TEXTURE0)`, use `DP_GL(glActiveTexture, GLTEXTURE0)`.
 * This only works for OpenGL functions with a `void` result and at least one
 * parameter (which is most of them). Otherwise, you either need to use the
 * `DP_GL_ASSIGN` macro or do a manual check with `DP_GL_CLEAR_ERROR` after your
 * function call. For functions with no parameters, use `DP_GL_0`.
 */
#    define DP_GL(FUNC, ...)          \
        do {                          \
            FUNC(__VA_ARGS__);        \
            DP_GL_CHECK_ERROR(#FUNC); \
        } while (0)
/*
 * Same as `DP_GL`, but for functions without arguments.
 */
#    define DP_GL_0(FUNC)             \
        do {                          \
            FUNC();                   \
            DP_GL_CHECK_ERROR(#FUNC); \
        } while (0)
#else
#    define DP_GL_CLEAR_ERROR() ((void)0)
#    define DP_GL(FUNC, ...)    FUNC(__VA_ARGS__)
#    define DP_GL_0(FUNC)       FUNC()
#endif


bool DP_gl_init(void);

void DP_gl_clear(float r, float g, float b, float a, float depth, int stencil);

unsigned int DP_gl_shader_new(unsigned int type, const char *source);

unsigned int DP_gl_program_new(const char *vert, const char *frag);

unsigned int DP_gl_get_error(void);

const char *DP_gl_strerror(unsigned int err);

void DP_gl_warn(unsigned int err, const char *where);

void DP_gl_panic(unsigned int err, const char *where);


#endif
