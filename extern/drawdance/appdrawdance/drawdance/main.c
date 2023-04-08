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
#include "emproxy.h"
#include "gl.h"
#include <dpcommon/common.h>
#include <SDL.h>
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>

#ifdef DRAWDANCE_IMGUI
#    include <gui.h>
#endif


static void show_init_error(const char *fmt, ...) DP_FORMAT(1, 2);
static void show_init_error(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    char *message = DP_vformat(fmt, ap);
    va_end(ap);
    DP_warn("%s", message);
    // SDL tries to make this function work even if initialization of SDL itself
    // fails, so it's okay that we try to call it here to show the error.
    int ok = SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
                                      "Drawdance Startup Error", message, NULL);
    if (ok != 0) {
        DP_warn("Couldn't show message box: %s", SDL_GetError());
    }
    DP_free(message);
}


#define SET_SDL_GL_ATTR(KEY, VALUE)                                  \
    do {                                                             \
        if (SDL_GL_SetAttribute(KEY, VALUE) != 0) {                  \
            DP_warn("Error setting SDL GL attribute '%s': %s", #KEY, \
                    SDL_GetError());                                 \
        }                                                            \
    } while (0)

static void set_default_sdl_config(void)
{
    // By default, SDL will use native OpenGL ES 2.0 if the driver supports it,
    // but drivers have a very strange sense on what "support" means. Instead,
    // we want to use ANGLE on Windows, which translates the OpenGL calls to use
    // DirectX under the hood, giving us a consistent experience. Setting this
    // confusingly-named hint is how that's accomplished.
#ifdef _WIN32
    SDL_SetHint(SDL_HINT_OPENGL_ES_DRIVER, "1");
#endif
    // Enable double-buffering and OpenGL ES 2.0. Warn but keep going if any of
    // these attributes can't be set, we'll just hope for the best in that case.
    SET_SDL_GL_ATTR(SDL_GL_DOUBLEBUFFER, 1);
    SET_SDL_GL_ATTR(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SET_SDL_GL_ATTR(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SET_SDL_GL_ATTR(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
}

static bool init_sdl(void)
{
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        show_init_error("Error initializing SDL: %s", SDL_GetError());
        return false;
    }
    set_default_sdl_config();
    return true;
}


static bool is_non_empty_string(const char *s)
{
    return s != NULL && s[0];
}

static void load_config_from(lua_State *L, const char *path)
{
    DP_debug("Attempting to load configuration from '%s'", path);
    if (luaL_loadfilex(L, path, "t") != LUA_OK
        || lua_pcall(L, 0, 0, 0) != LUA_OK) {
        // TODO: should probably distinguish between errors here. Not being able
        // to open the file is fine, but a syntax error should be fatal.
        DP_warn("Could not read configuration from '%s': %s", path,
                lua_tostring(L, -1));
        lua_pop(L, 1);
    }
}

static bool load_config(void)
{
    // We'll use a bare Lua state to load our configuration. It doesn't have any
    // libraries loaded, so the only bad thing a user could do with it is to run
    // an infinite loop. Saves us from using an INI parser or whatever.
    lua_State *L = luaL_newstate();
    if (!L) {
        show_init_error("Failed to create Lua state to read configuration");
        return false;
    }

    const char *explicit_config_path = SDL_getenv("DRAWDANCE_CONFIG_PATH");
    if (is_non_empty_string(explicit_config_path)) {
        load_config_from(L, explicit_config_path);
    }
    else {
        char *pref_path = SDL_GetPrefPath("drawpile", "drawdance");
        if (is_non_empty_string(pref_path)) {
            char *path = DP_format("%sconfig.lua", pref_path);
            load_config_from(L, path);
            DP_free(path);
        }
        SDL_free(pref_path);
    }

    // TODO: actually load configuration here

    lua_close(L);
    return true;
}


static SDL_Window *open_window(void)
{
    SDL_Window *window = SDL_CreateWindow(
        "Drawdance", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 1280,
        720, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (!window) {
        show_init_error("Error opening window: %s", SDL_GetError());
        return NULL;
    }
    return window;
}


static SDL_GLContext create_gl_context(SDL_Window *window)
{
    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    if (!gl_context) {
        show_init_error("Error creating OpenGL context: %s", SDL_GetError());
        return NULL;
    }
    return gl_context;
}


#ifdef DRAWDANCE_IMGUI
static bool init_imgui_sdl_gl(SDL_Window *window, SDL_GLContext gl_context)
{
    if (DP_imgui_init_sdl(window, gl_context)) {
        if (DP_imgui_init_gl()) {
            return true;
        }
        DP_imgui_quit_sdl();
    }
    return false;
}

static bool init_imgui(SDL_Window *window, SDL_GLContext gl_context)
{
    if (DP_imgui_init()) {
        if (DP_EMPROXY_III(init_imgui_sdl_gl, window, gl_context)) {
            return true;
        }
        DP_imgui_quit();
    }
    show_init_error("Error initializing Dear ImGUI");
    return false;
}

static void quit_imgui_sdl_gl(void)
{
    DP_imgui_quit_gl();
    DP_imgui_quit_sdl();
}
#endif

static DP_App *init_app(SDL_Window *window, SDL_GLContext gl_context)
{
    DP_App *app = DP_app_new(window, gl_context);
    if (!app) {
        show_init_error("Error starting application: %s", DP_error());
    }
    return app;
}

int main(void)
{
    bool sdl_initialized = false;
    SDL_Window *window = NULL;
    SDL_GLContext gl_context = NULL;
#ifdef DRAWDANCE_IMGUI
    bool imgui_initialized = false;
#endif
    DP_App *app = NULL;

    bool ok = (sdl_initialized = init_sdl())           //
           && load_config()                            //
           && (window = open_window())                 //
           && (gl_context = create_gl_context(window)) //
           && DP_EMPROXY_I(DP_gl_init)                 //
#ifdef DRAWDANCE_IMGUI
           && (imgui_initialized = init_imgui(window, gl_context)) //
#endif
           && (app = init_app(window, gl_context)); //

    if (ok) {
        DP_app_run(app);
#ifdef __EMSCRIPTEN__
        // The browser controls the main loop, we are just a callback.
        return EXIT_SUCCESS;
#endif
    }

    if (app) {
        DP_app_free(app);
    }
#ifdef DRAWDANCE_IMGUI
    if (imgui_initialized) {
        DP_EMPROXY_V(quit_imgui_sdl_gl);
        DP_imgui_quit();
    }
#endif
    if (gl_context) {
        SDL_GL_DeleteContext(gl_context);
    }
    if (window) {
        SDL_DestroyWindow(window);
    }
    if (sdl_initialized) {
#ifndef DP_ADDRESS_SANITIZER
        // Calling this function confuses Address Sanitizer into reporting loads
        // of untraceable indirect leaks in our application. Since not calling
        // it is fine, we'll just skip it so that the leak report remains clean.
        SDL_Quit();
#endif
    }
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
