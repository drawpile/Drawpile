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
#include "app.h"
#include "canvas_renderer.h"
#include "dpclient/client.h"
#include "emproxy.h"
#include "gl.h"
#include "ui.h"
#include <dpclient/document.h>
#include <dpcommon/common.h>
#include <dpcommon/input.h>
#include <dpcommon/threading.h>
#include <dpcommon/worker.h>
#include <dpengine/canvas_diff.h>
#include <dpengine/canvas_state.h>
#include <dpengine/layer_content.h>
#include <dpmsg/binary_reader.h>
#include <dpmsg/message.h>
#include <SDL.h>
#include <lauxlib.h>
#include <lua.h>
#include <lua_bindings.h>
#include <lualib.h>

#ifdef DRAWDANCE_IMGUI
#    include <gui.h>
#endif

#ifdef __EMSCRIPTEN__
#    include <emscripten.h>
#endif


#define HANDLE_EVENTS_QUIT         0
#define HANDLE_EVENTS_KEEP_RUNNING 1

struct DP_App {
    bool running;
    SDL_Window *window;
    SDL_GLContext gl_context;
    DP_Document *doc;
    DP_CanvasState *blank_state;
    DP_CanvasState *previous_state;
    DP_TransientLayerContent *tlc;
    DP_CanvasDiff *diff;
    DP_CanvasRenderer *canvas_renderer;
    DP_LuaWarnBuffer lua_warn_buffer;
    lua_State *L;
    int lua_app_ref;
#if defined(DRAWDANCE_IMGUI) && !defined(__EMSCRIPTEN__)
    // Don't parallelize this in the browser, it's way slower than in serial.
    DP_Semaphore *sem_gui_prepare;
    DP_Semaphore *sem_gui_render;
    DP_Thread *thread_gui;
#endif
    DP_Worker *worker;
    DP_UserInputs inputs;
};

typedef struct DP_AppWorkerJobParams {
    DP_AppWorkerJobFn fn;
    void *user;
} DP_AppWorkerJobParams;


static bool init_canvas_renderer(DP_App *app)
{
    DP_CanvasRenderer *cr = DP_EMPROXY_P(DP_canvas_renderer_new);
    if (cr) {
        app->canvas_renderer = cr;
        app->diff = DP_canvas_diff_new();
        app->tlc = DP_transient_layer_content_new_init(0, 0, NULL);
        app->blank_state = DP_canvas_state_new();
        return true;
    }
    else {
        return false;
    }
}

static bool init_lua(DP_App *app)
{
    lua_State *L = luaL_newstate();
    if (!L) {
        DP_error_set("Can't open Lua state");
        return false;
    }

    if (!DP_lua_bindings_init(L, app, &app->lua_warn_buffer)) {
        lua_close(L);
        return false;
    }

    int lua_app_ref = DP_lua_app_new(L);
    if (lua_app_ref == LUA_NOREF) {
        lua_close(L);
        return false;
    }

    app->L = L;
    app->lua_app_ref = lua_app_ref;
    return true;
}


#if defined(DRAWDANCE_IMGUI) && !defined(__EMSCRIPTEN__)
static void run_gui_thread(void *arg)
{
    DP_App *app = arg;
    lua_State *L = app->L;
    int lua_app_ref = app->lua_app_ref;
    DP_Semaphore *sem_prepare = app->sem_gui_prepare;
    DP_Semaphore *sem_render = app->sem_gui_render;
    while (true) {
        DP_SEMAPHORE_MUST_WAIT(sem_prepare);
        if (app->running) {
            DP_lua_app_prepare_gui(L, lua_app_ref);
            DP_SEMAPHORE_MUST_POST(sem_render);
        }
        else {
            break;
        }
    }
}

static bool init_gui_thread(DP_App *app)
{
    return (app->sem_gui_prepare = DP_semaphore_new(0))
        && (app->sem_gui_render = DP_semaphore_new(0))
        && (app->thread_gui = DP_thread_new(run_gui_thread, app));
}
#else
static bool init_gui_thread(DP_UNUSED DP_App *app)
{
    return true; // ImGui disabled or not in a separate thread.
}
#endif

static void run_app_worker_job(void *element, DP_UNUSED int thread_index)
{
    DP_AppWorkerJobParams *params = element;
    params->fn(params->user);
}

static bool init_worker(DP_App *app)
{
    return (app->worker = DP_worker_new(1, sizeof(DP_AppWorkerJobParams), 1,
                                        run_app_worker_job));
}


static int handle_events(DP_App *app)
{
    DP_user_inputs_next_frame(&app->inputs);
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        DP_user_inputs_handle(&app->inputs, &event);
#ifdef DRAWDANCE_IMGUI
        DP_imgui_handle_event(&event);
#endif
        if (event.type == SDL_QUIT) {
            DP_debug("Ate SDL_QUIT event, exiting");
            return HANDLE_EVENTS_QUIT;
        }
    }

    DP_LayerPropsList *lpl =
        DP_canvas_diff_layer_props_changed_reset(app->diff)
            ? DP_canvas_state_layer_props_noinc(app->previous_state)
            : NULL;

    DP_lua_app_handle_events(app->L, app->lua_app_ref, lpl);
    return HANDLE_EVENTS_KEEP_RUNNING;
}

#ifdef DRAWDANCE_IMGUI
void new_imgui_frame_gl(void)
{
    DP_imgui_new_frame_gl();
    DP_GL_CLEAR_ERROR();
}

static void prepare_gui(DP_App *app)
{
    DP_EMPROXY_V(new_imgui_frame_gl);
    DP_imgui_new_frame();
#    ifdef __EMSCRIPTEN__
    DP_lua_app_prepare_gui(app->L, app->lua_app_ref);
#    else
    DP_SEMAPHORE_MUST_POST(app->sem_gui_prepare);
#    endif
}
#endif

static DP_CanvasDiff *prepare_canvas(DP_App *app)
{
    DP_Document *doc = app->doc;
    DP_CanvasState *prev = app->previous_state;
    DP_CanvasState *next =
        doc ? DP_document_canvas_state_compare_and_get(doc, prev)
            : DP_canvas_state_incref(app->blank_state);
    if (next) {
        DP_CanvasDiff *diff = app->diff;
        DP_canvas_state_diff(next, prev, diff);
        if (prev) {
            DP_canvas_state_decref(prev);
        }
        app->previous_state = next;
        app->tlc = DP_canvas_state_render(next, app->tlc, diff);
        return diff;
    }
    else {
        return NULL;
    }
}

static void clear_screen(void)
{
    // TODO: load clear color
    DP_gl_clear(0.5f, 0.5f, 0.5f, 1.0f, 0.0f, 0);
}

static void render_canvas(DP_App *app, DP_CanvasDiff *diff_or_null)
{
    int view_width, view_height;
    SDL_GL_GetDrawableSize(app->window, &view_width, &view_height);
    DP_user_input_view_dimensions_set(&app->inputs, view_width, view_height);
    DP_canvas_renderer_render(app->canvas_renderer, (DP_LayerContent *)app->tlc,
                              view_width, view_height, diff_or_null);
}

#ifdef DRAWDANCE_IMGUI
static void render_gui(DP_App *app)
{
    DP_imgui_render(app->window, app->gl_context);
    DP_GL_CLEAR_ERROR();
}
#endif


DP_App *DP_app_new(SDL_Window *window, SDL_GLContext gl_context)
{
    DP_App *app = DP_malloc(sizeof(*app));
    *app = (DP_App)
    {
        true, window, gl_context, NULL, NULL, NULL, NULL, NULL, NULL,
            {0, 0, NULL}, NULL, LUA_NOREF,
#if defined(DRAWDANCE_IMGUI) && !defined(__EMSCRIPTEN__)
            NULL, NULL, NULL,
#endif
            NULL,
        {
            0
        }
    };
    DP_user_inputs_init(&app->inputs);

    bool ok = init_canvas_renderer(app) //
           && init_lua(app)             //
           && init_gui_thread(app)      //
           && init_worker(app);

    if (ok) {
        return app;
    }
    else {
        DP_app_free(app);
        return NULL;
    }
}

void DP_app_free(DP_App *app)
{
    if (app) {
        app->running = false;
        DP_Worker *worker = app->worker;
        app->worker = NULL;
        DP_worker_free_join(worker);
#if defined(DRAWDANCE_IMGUI) && !defined(__EMSCRIPTEN__)
        if (app->thread_gui) {
            DP_SEMAPHORE_MUST_POST(app->sem_gui_prepare);
            DP_thread_free_join(app->thread_gui);
        }
        DP_semaphore_free(app->sem_gui_render);
        DP_semaphore_free(app->sem_gui_prepare);
#endif
        DP_lua_app_free(app->L, app->lua_app_ref);
        if (app->L) {
            lua_close(app->L);
        }
        DP_lua_warn_buffer_dispose(&app->lua_warn_buffer);
        DP_canvas_renderer_free(app->canvas_renderer);
        DP_canvas_diff_free(app->diff);
        if (app->tlc) {
            DP_transient_layer_content_decref(app->tlc);
        }
        if (app->previous_state) {
            DP_canvas_state_decref(app->previous_state);
        }
        if (app->blank_state) {
            DP_canvas_state_decref(app->blank_state);
        }
        DP_user_inputs_dispose(&app->inputs);
        DP_free(app);
    }
}

#ifdef __EMSCRIPTEN__
static void em_render_gl(DP_App *app, DP_CanvasDiff *diff_or_null)
{
    clear_screen();
    render_canvas(app, diff_or_null);
#    ifdef DRAWDANCE_IMGUI
    render_gui(app);
#    endif
    DP_user_inputs_render(&app->inputs);
    SDL_GL_SwapWindow(app->window);
}

static void em_main_loop(void *arg)
{
    DP_App *app = arg;
    if (handle_events(app) != HANDLE_EVENTS_QUIT) {
#    ifdef DRAWDANCE_IMGUI
        prepare_gui(app);
#    endif
        DP_CanvasDiff *diff_or_null = prepare_canvas(app);
        DP_EMPROXY_VII(em_render_gl, app, diff_or_null);
    }
    else {
        DP_app_free(app);
        emscripten_cancel_main_loop();
    }
}
#endif

void DP_app_run(DP_App *app)
{
#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop_arg(em_main_loop, app, 0, false);
#else
    while (handle_events(app) != HANDLE_EVENTS_QUIT) {
        DP_GL_CLEAR_ERROR();
#    ifdef DRAWDANCE_IMGUI
        prepare_gui(app);
#    endif
        DP_CanvasDiff *diff_or_null = prepare_canvas(app);
        clear_screen();
        render_canvas(app, diff_or_null);
#    ifdef DRAWDANCE_IMGUI
        DP_SEMAPHORE_MUST_WAIT(app->sem_gui_render);
#    endif
#    ifdef DRAWDANCE_IMGUI
        render_gui(app);
#    endif
        DP_user_inputs_render(&app->inputs);
        SDL_GL_SwapWindow(app->window);
    }
#endif
}

void DP_app_document_set(DP_App *app, DP_Document *doc_or_null)
{
    DP_ASSERT(app);
    DP_debug("App document set to %p", (void *)doc_or_null);
    app->doc = doc_or_null;
}

void DP_app_canvas_renderer_transform(DP_App *app, double *out_x, double *out_y,
                                      double *out_scale,
                                      double *out_rotation_in_radians)
{
    DP_ASSERT(app);
    DP_canvas_renderer_transform(app->canvas_renderer, out_x, out_y, out_scale,
                                 out_rotation_in_radians);
}

void DP_app_canvas_renderer_transform_set(DP_App *app, double x, double y,
                                          double scale,
                                          double rotation_in_radians)
{
    DP_ASSERT(app);
    DP_canvas_renderer_transform_set(app->canvas_renderer, x, y, scale,
                                     rotation_in_radians);
}

bool DP_app_worker_ready(DP_App *app)
{
    DP_ASSERT(app);
    return app->worker != NULL;
}

void DP_app_worker_push(DP_App *app, DP_AppWorkerJobFn fn, void *user)
{
    DP_ASSERT(app);
    DP_ASSERT(fn);
    DP_ASSERT(app->worker);
    DP_AppWorkerJobParams params = {fn, user};
    DP_worker_push(app->worker, &params);
}

DP_UserInputs *DP_app_inputs(DP_App *app)
{
    DP_ASSERT(app);
    return &app->inputs;
}
