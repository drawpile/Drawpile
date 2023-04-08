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
#ifndef DRAWDANCE_EMPROXY_H
#define DRAWDANCE_EMPROXY_H
#include <dpcommon/common.h>

/*
 * By default, Emscripten runs code in a worker thread (because we link with
 * -s PROXY_TO_PTHREAD) so that locking on mutexes and waiting on semaphores
 * doesn't block the main browser thread, which according to Emscripten's
 * documentation is really bad. However, there's some stuff that has to run
 * on the main browser thread, the relevant thing for us being OpenGL calls.
 *
 * These macros proxy functions to the main browser thread, blocking the calling
 * thread until the call has completed. They should only be used in main.c and
 * app.c, otherwise it's gonna get really confusing as to where stuff is being
 * executed.
 *
 * The macros are pretty ugly, but basically, they encode the return value and
 * argument types in their name. For example, DP_EMPROXY_VI returns void (V) and
 * takes a single 32 bit parameter (I). DP_EMPROXY_III returns a 32 bit value
 * and takes in two of them. Those 32 bit things are implemented as int, but
 * will also work with bool and pointers. For convenience, there's also a return
 * type P here that doesn't exist in the Emscripten macros. It casts the
 * returned 32 bit value to a void * instead of leaving it as an int.
 *
 * If you're not on Emscripten, the macros just call the function directly.
 */

#ifdef __EMSCRIPTEN__
#    include <emscripten/threading.h>

// Just a shortcut to that enormously long function name.
#    define DP_EMPROXY emscripten_sync_run_in_main_runtime_thread

// Function returning void and taking no parameters.
#    define DP_EMPROXY_V(FN) DP_EMPROXY(EM_FUNC_SIG_V, FN)

// Function returning void and taking one 32 bit parameter.
#    define DP_EMPROXY_VI(FN, A) DP_EMPROXY(EM_FUNC_SIG_VI, FN, A)

// Function returning void and taking two 32 bit parameters.
#    define DP_EMPROXY_VII(FN, A, B) DP_EMPROXY(EM_FUNC_SIG_VII, FN, A, B)

// Function returning a 32 bit value and taking no parameters.
#    define DP_EMPROXY_I(FN) DP_EMPROXY(EM_FUNC_SIG_I, FN)

// Function returning a 32 bit value and taking two 32 bit parameters.
#    define DP_EMPROXY_III(FN, A, B) DP_EMPROXY(EM_FUNC_SIG_III, FN, A, B)

// Function returning a 32 bit value cast to a void * and taking no parameters.
#    define DP_EMPROXY_P(FN) ((void *)DP_EMPROXY_I(FN))

#else
#    define DP_EMPROXY_V(FN)         FN()
#    define DP_EMPROXY_VI(FN, A)     FN(A)
#    define DP_EMPROXY_VII(FN, A, B) FN(A, B)
#    define DP_EMPROXY_I(FN)         FN()
#    define DP_EMPROXY_III(FN, A, B) FN(A, B)
#    define DP_EMPROXY_P(FN)         FN()
#endif

#endif
