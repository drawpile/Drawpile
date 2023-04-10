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
#include "lua_bindings.h"
#include "../drawdance/app.h"
#include <dpclient/client.h>
#include <dpcommon/common.h>
#include <dpcommon/conversions.h>
#include <dpcommon/file.h>
#include <dpcommon/queue.h>
#include <dpcommon/threading.h>
#include <dpengine/layer_props.h>
#include <dpengine/layer_props_list.h>
#include <dpmsg/message.h>
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>

#ifdef __EMSCRIPTEN__
#    include <emscripten.h>
#    include <emscripten/threading.h>
#endif


typedef struct DP_Document DP_Document;


#ifdef _WIN32
#    define PATH_SEPARATOR "\\"
#else
#    define PATH_SEPARATOR "/"
#endif

#define CLIENT_EVENT_MESSAGE (-1)
#ifdef __EMSCRIPTEN__
#    define BROWSER_EVENTS (-2)
#endif


typedef struct DP_LuaAppState {
    DP_App *app;
    DP_Mutex *mutex_client_event_queue;
    DP_Queue client_event_queue;
} DP_LuaAppState;

typedef struct DP_LuaClientEvent {
    int client_id;
    int type;
    void *data;
} DP_LuaClientEvent;

static void lua_client_event_dispose(void *element)
{
    DP_LuaClientEvent *lce = element;
    switch (lce->type) {
    case CLIENT_EVENT_MESSAGE:
        if (lce->data) { // Moving to Lua sets this to null.
            DP_message_decref(lce->data);
        }
        break;
#ifdef __EMSCRIPTEN__
    case BROWSER_EVENTS:
        free(lce->data);
        break;
#endif
    default:
        DP_free(lce->data);
        break;
    }
}

static void push_client_event(DP_LuaAppState *state, int client_id, int type,
                              void *data)
{
    DP_ASSERT(state);
    DP_Mutex *mutex_client_event_queue = state->mutex_client_event_queue;
    DP_MUTEX_MUST_LOCK(mutex_client_event_queue);
    DP_LuaClientEvent *lce =
        DP_queue_push(&state->client_event_queue, sizeof(DP_LuaClientEvent));
    *lce = (DP_LuaClientEvent){client_id, type, data};
    DP_MUTEX_MUST_UNLOCK(mutex_client_event_queue);
}

void DP_lua_app_state_client_event_push(DP_LuaAppState *state, int client_id,
                                        DP_ClientEventType type,
                                        const char *message_or_null)
{
    push_client_event(state, client_id, (int)type, DP_strdup(message_or_null));
}

void DP_lua_app_state_client_message_push(DP_LuaAppState *state, int client_id,
                                          DP_Message *msg)
{
    push_client_event(state, client_id, CLIENT_EVENT_MESSAGE,
                      DP_message_incref(msg));
}

void DP_lua_message_push_noinc(lua_State *L, DP_Message *msg);

static int publish_client_events(lua_State *L)
{
    DP_ASSERT(lua_gettop(L) == 2);
    DP_ASSERT(lua_type(L, 2) == LUA_TTABLE); // The Lua App instance.
    DP_Queue *client_event_queue = lua_touserdata(L, 1);
    lua_getfield(L, 2, "publish"); // Grab the method, we'll need it.
    DP_LuaClientEvent *lce;
    while ((lce = DP_queue_peek(client_event_queue, sizeof(*lce)))) {
        int type = lce->type;
        if (type == CLIENT_EVENT_MESSAGE) {
            lua_pushvalue(L, 3); // The publish method.
            lua_pushvalue(L, 2); // The Lua App instance (self).
            lua_pushliteral(L, "CLIENT_MESSAGE");
            lua_createtable(L, 0, 2);
            lua_pushinteger(L, lce->client_id);
            lua_setfield(L, -2, "client_id");
            // Steal the reference to avoid messing with the refcount.
            DP_lua_message_push_noinc(L, lce->data);
            lce->data = NULL; // Stolen.
            lua_setfield(L, -2, "message");
            lua_call(L, 3, 0);
        }
#ifdef __EMSCRIPTEN__
        else if (type == BROWSER_EVENTS) {
            lua_getfield(L, 2, "publish_browser_events");
            lua_pushvalue(L, 2); // The Lua App instance (self).
            lua_pushstring(L, lce->data);
            lua_call(L, 2, 0);
        }
#endif
        else {
            lua_pushvalue(L, 3); // The publish method.
            lua_pushvalue(L, 2); // The Lua App instance (self).
            lua_pushliteral(L, "CLIENT_EVENT");
            lua_createtable(L, 0, 3);
            lua_pushinteger(L, lce->client_id);
            lua_setfield(L, -2, "client_id");
            lua_pushinteger(L, type);
            lua_setfield(L, -2, "type");
            lua_pushstring(L, lce->data);
            lua_setfield(L, -2, "message");
            lua_call(L, 3, 0);
        }
        lua_client_event_dispose(lce);
        DP_queue_shift(client_event_queue);
    }
    return 0;
}

static void move_client_events_to_lua(lua_State *L, int ref)
{
    DP_LuaAppState *state = DP_lua_app_state(L);
    DP_Mutex *mutex_client_event_queue = state->mutex_client_event_queue;
    lua_pushcfunction(L, publish_client_events);
    lua_pushlightuserdata(L, &state->client_event_queue);
    lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
    DP_MUTEX_MUST_LOCK(mutex_client_event_queue);
    int result = lua_pcall(L, 2, 0, 0);
    DP_MUTEX_MUST_UNLOCK(mutex_client_event_queue);
    if (result != LUA_OK) {
        DP_warn("Error publishing client events: %s", lua_tostring(L, -1));
        lua_pop(L, 1);
    }
}

static void push_layer_props_list(lua_State *L, DP_LayerPropsList *lpl)
{
    int count = DP_layer_props_list_count(lpl);
    lua_createtable(L, count, 0);
    for (int i = 0; i < count; ++i) {
        DP_LayerProps *lp = DP_layer_props_list_at_noinc(lpl, i);
        DP_LayerPropsList *child_lpl = DP_layer_props_children_noinc(lp);
        lua_createtable(L, 0, child_lpl ? 8 : 7);
        lua_pushinteger(L, DP_layer_props_id(lp));
        lua_setfield(L, -2, "id");
        lua_pushinteger(L, DP_layer_props_opacity(lp));
        lua_setfield(L, -2, "opacity");
        lua_pushinteger(L, DP_layer_props_blend_mode(lp));
        lua_setfield(L, -2, "blend_mode");
        lua_pushboolean(L, DP_layer_props_hidden(lp));
        lua_setfield(L, -2, "hidden");
        lua_pushboolean(L, DP_layer_props_censored(lp));
        lua_setfield(L, -2, "censored");
        lua_pushboolean(L, DP_layer_props_isolated(lp));
        lua_setfield(L, -2, "isolated");
        size_t title_length;
        const char *title = DP_layer_props_title(lp, &title_length);
        lua_pushlstring(L, title, title_length);
        lua_setfield(L, -2, "title");
        if (child_lpl) {
            push_layer_props_list(L, child_lpl);
            lua_setfield(L, -2, "children");
        }
        lua_seti(L, -2, i + 1);
    }
}

static int publish_layer_props_event(lua_State *L)
{
    DP_ASSERT(lua_gettop(L) == 2);
    DP_ASSERT(lua_type(L, 1) == LUA_TLIGHTUSERDATA); // Layer props list.
    DP_ASSERT(lua_type(L, 2) == LUA_TTABLE);         // The Lua App instance.
    lua_getfield(L, 2, "publish");
    lua_pushvalue(L, 2);
    lua_pushliteral(L, "LAYER_PROPS");
    push_layer_props_list(L, lua_touserdata(L, 1));
    lua_call(L, 3, 0);
    return 0;
}

static void create_layer_props_event(lua_State *L, int ref,
                                     DP_LayerPropsList *lpl)
{
    DP_debug("Publishing layer props event");
    lua_pushcfunction(L, publish_layer_props_event);
    lua_pushlightuserdata(L, lpl);
    lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
    int result = lua_pcall(L, 2, 0, 0);
    if (result != LUA_OK) {
        DP_warn("Error publishing layer props event: %s", lua_tostring(L, -1));
        lua_pop(L, 1);
    }
}


static int lua_app_state_gc(lua_State *L)
{
    DP_LuaAppState *state = lua_touserdata(L, 1);
    DP_queue_clear(&state->client_event_queue, sizeof(DP_LuaClientEvent),
                   lua_client_event_dispose);
    DP_queue_dispose(&state->client_event_queue);
    DP_mutex_free(state->mutex_client_event_queue);
    return 0;
}

static int init_lua_app_state(lua_State *L)
{
    luaL_newmetatable(L, "DP_LuaAppState");
    lua_pushcfunction(L, lua_app_state_gc);
    lua_setfield(L, -2, "__gc");
    lua_pop(L, 1);

    DP_App *app = lua_touserdata(L, 1);
    DP_LuaAppState *state = lua_newuserdatauv(L, sizeof(*state), 0);
    *state = (DP_LuaAppState){app, NULL, DP_QUEUE_NULL};
    luaL_setmetatable(L, "DP_LuaAppState");

    if (!(state->mutex_client_event_queue = DP_mutex_new())) {
        return luaL_error(L, "Can't create client event queue mutex: %s",
                          DP_error());
    }

    DP_queue_init(&state->client_event_queue, 8, sizeof(DP_LuaClientEvent));

    luaL_ref(L, LUA_REGISTRYINDEX);
    *(DP_LuaAppState **)lua_getextraspace(L) = state;
    return 0;
}

static int init_lua_libs(lua_State *L)
{
    static const luaL_Reg libraries[] = {
        {LUA_GNAME, luaopen_base},
        {LUA_COLIBNAME, luaopen_coroutine},
        {LUA_TABLIBNAME, luaopen_table},
        {LUA_STRLIBNAME, luaopen_string},
        {LUA_MATHLIBNAME, luaopen_math},
        {LUA_UTF8LIBNAME, luaopen_utf8},
        {NULL, NULL},
    };
    for (const luaL_Reg *lib = libraries; lib->func; lib++) {
        luaL_requiref(L, lib->name, lib->func, 1);
        lua_pop(L, 1);
    }
    return 0;
}

static int version(lua_State *L)
{
    lua_pushliteral(L, DP_VERSION);
    return 1;
}

static int build_type(lua_State *L)
{
    lua_pushliteral(L, DP_BUILD_TYPE);
    return 1;
}

static int platform(lua_State *L)
{
    lua_pushliteral(L, DP_PLATFORM);
    return 1;
}

static int open_url(lua_State *L)
{
    const char *url = luaL_checkstring(L, 1);
    if (SDL_OpenURL(url) == 0) {
        return 0;
    }
    else {
        return luaL_error(L, "Error opening '%s': %s", url, SDL_GetError());
    }
}

static int slurp(lua_State *L)
{
    const char *path = luaL_checkstring(L, 1);
    size_t length;
    char *content = DP_file_slurp(path, &length);
    if (content) {
        lua_pushlstring(L, content, length);
        DP_free(content);
        return 1;
    }
    else {
        return luaL_error(L, "%s", DP_error());
    }
}

#ifdef __EMSCRIPTEN__
EM_JS(void, dispatch_custom_event, (lua_State * L, const char *payload), {
    try {
        window.dispatchEvent(new CustomEvent("dpevents", {
            detail : {
                handle : L,
                payload : UTF8ToString(payload),
            },
        }));
    }
    catch (e) {
        console.error("Error dispatching dpevents: " + e);
    }
})

static void send_to_browser_in_main_thread(lua_State *L, char *payload)
{
    dispatch_custom_event(L, payload);
    DP_free(payload);
}

static int send_to_browser(lua_State *L)
{
    size_t length;
    const char *events = luaL_checklstring(L, 1, &length);
    char *payload = DP_malloc(length + 1);
    memcpy(payload, events, length);
    payload[length] = '\0';
    emscripten_async_run_in_main_runtime_thread(
        EM_FUNC_SIG_VII, send_to_browser_in_main_thread, L, payload);
    return 0;
}

// Exported function called from JavaScript.
void DP_send_from_browser(lua_State *L, char *payload)
{
    DP_LuaAppState *state = DP_lua_app_state(L);
    push_client_event(state, 0, BROWSER_EVENTS, payload);
}
#endif

static int error_to_trace(lua_State *L)
{
    const char *err = lua_tostring(L, 1);
    luaL_traceback(L, L, err, 0);
    return 1;
}

static int app_quit(DP_UNUSED lua_State *L)
{
    SDL_Event event;
    event.type = SDL_QUIT;
    if (SDL_PushEvent(&event) < 0) {
        luaL_error(L, "Error sending quit event: %s", SDL_GetError());
    }
    return 0;
}

static int app_document_set(lua_State *L)
{
    luaL_checkany(L, 1);
    DP_Document *doc;
    if (lua_isnil(L, 1)) {
        doc = NULL;
    }
    else {
        DP_Document **pp = luaL_checkudata(L, 1, "DP_Document");
        doc = pp ? *pp : NULL;
    }
    DP_LuaAppState *state = DP_lua_app_state(L);
    DP_app_document_set(state->app, doc);
    return 0;
}

static int app_canvas_renderer_transform(lua_State *L)
{
    DP_LuaAppState *state = DP_lua_app_state(L);
    double x, y, scale, rotation_in_radians;
    DP_app_canvas_renderer_transform(state->app, &x, &y, &scale,
                                     &rotation_in_radians);
    lua_pushnumber(L, x);
    lua_pushnumber(L, y);
    lua_pushnumber(L, scale);
    lua_pushnumber(L, rotation_in_radians);
    return 4;
}

static int app_canvas_renderer_transform_set(lua_State *L)
{
    double x = luaL_checknumber(L, 1);
    double y = luaL_checknumber(L, 2);
    double scale = luaL_checknumber(L, 3);
    double rotation_in_radians = luaL_checknumber(L, 4);
    DP_LuaAppState *state = DP_lua_app_state(L);
    DP_app_canvas_renderer_transform_set(state->app, x, y, scale,
                                         rotation_in_radians);
    return 0;
}

static int init_app_funcs(lua_State *L)
{
    lua_pushglobaltable(L);
    luaL_getsubtable(L, -1, "DP");

    lua_pushcfunction(L, error_to_trace);
    lua_setfield(L, -2, "error_to_trace");
    lua_pushcfunction(L, version);
    lua_setfield(L, -2, "version");
    lua_pushcfunction(L, build_type);
    lua_setfield(L, -2, "build_type");
    lua_pushcfunction(L, platform);
    lua_setfield(L, -2, "platform");
    lua_pushcfunction(L, open_url);
    lua_setfield(L, -2, "open_url");
    lua_pushcfunction(L, slurp);
    lua_setfield(L, -2, "slurp");
#ifdef __EMSCRIPTEN__
    lua_pushcfunction(L, send_to_browser);
    lua_setfield(L, -2, "send_to_browser");
#endif

    luaL_getsubtable(L, -1, "App");
    lua_pushcfunction(L, app_quit);
    lua_setfield(L, -2, "quit");
    lua_pushcfunction(L, app_document_set);
    lua_setfield(L, -2, "document_set");
    lua_pushcfunction(L, app_canvas_renderer_transform);
    lua_setfield(L, -2, "canvas_renderer_transform");
    lua_pushcfunction(L, app_canvas_renderer_transform_set);
    lua_setfield(L, -2, "canvas_renderer_transform_set");

    return 0;
}


#ifdef DRAWDANCE_IMGUI
int ImGuiLua_InitImGui(lua_State *L);
int ImGuiLua_NewImVec2(lua_State *L);
#endif

static int init_imgui(lua_State *L)
{
    lua_pushglobaltable(L);
    luaL_getsubtable(L, -1, "DP");
#ifdef DRAWDANCE_IMGUI
    lua_pushboolean(L, true);
#else
    lua_pushboolean(L, false);
#endif
    lua_setfield(L, -2, "ImGui");
    lua_pop(L, 2);

#ifdef DRAWDANCE_IMGUI
    lua_pushcfunction(L, ImGuiLua_InitImGui);
    lua_pushglobaltable(L);
    lua_call(L, 1, 0);

    lua_newtable(L);
    lua_pushcfunction(L, ImGuiLua_NewImVec2);
    lua_setfield(L, -2, "new");
    lua_setglobal(L, "ImVec2");
#endif

    return 0;
}

static int require_load_file(lua_State *L)
{
    lua_concat(L, 3);
    DP_debug("Load '%s'", lua_tostring(L, 1));
    if (luaL_loadfilex(L, lua_tostring(L, 1), "t") != LUA_OK) {
        return lua_error(L);
    }
    int prev = lua_gettop(L);
    lua_call(L, 0, LUA_MULTRET);
    int count = lua_gettop(L) - prev + 1;
    lua_createtable(L, count, 0);
    for (int i = 0; i < count; ++i) {
        lua_pushvalue(L, prev + i);
        lua_seti(L, -2, i + 1);
    }
    return 1;
}

static int require(lua_State *L)
{
    const char *package = luaL_checkstring(L, 1);
    lua_getfield(L, LUA_REGISTRYINDEX, "DP_loaded_packages");
    int loaded = lua_gettop(L);
    int state = lua_getfield(L, loaded, package);
    if (state == LUA_TNIL) {
        // Remove the nil.
        lua_pop(L, 1);
        // Set package state to 0.
        lua_pushinteger(L, 0);
        lua_setfield(L, loaded, package);
        // Run the actual file. TODO: prefix and handle this path properly.
        lua_pushcfunction(L, require_load_file);
        lua_pushliteral(L, "appdrawdance/lua/");
        luaL_gsub(L, package, ".", PATH_SEPARATOR);
        lua_pushliteral(L, ".lua");
        if (lua_pcall(L, 3, 1, 0) == LUA_OK) {
            // Set package state to returned packed table.
            lua_pushvalue(L, -1);
            lua_setfield(L, loaded, package);
        }
        else {
            // Set package state to nil.
            lua_pushnil(L);
            lua_setfield(L, loaded, package);
            return lua_error(L);
        }
    }
    else if (state == LUA_TNUMBER) {
        return luaL_error(L, "Circular require of '%s'", package);
    }
    // Unpack the table.
    int table = lua_gettop(L);
    int count = (int)luaL_len(L, table);
    for (int i = 1; i <= count; ++i) {
        lua_geti(L, table, i);
    }
    return count;
}

static int unrequire(lua_State *L)
{
    luaL_checkany(L, 1);
    lua_getfield(L, LUA_REGISTRYINDEX, "DP_loaded_packages");
    int removed_count = 0;

    lua_pushnil(L);
    while (lua_next(L, -2)) {
        lua_pop(L, 1); // Don't care about the value, only the key matters.

        // If we were given a string, compare it to the package.
        // Otherwise try to call it and use the result.
        bool should_remove;
        if (lua_isstring(L, 1)) {
            should_remove = lua_compare(L, 1, -1, LUA_OPEQ);
        }
        else {
            lua_pushvalue(L, 1);
            lua_pushvalue(L, -2);
            lua_call(L, 1, 1);
            should_remove = lua_toboolean(L, -1);
            lua_pop(L, 1);
        }

        if (should_remove) {
            lua_pushvalue(L, -1);
            lua_pushnil(L);
            lua_settable(L, -4);
            ++removed_count;
        }
    }

    lua_pushinteger(L, removed_count);
    return 1;
}

static int init_package(lua_State *L)
{
    lua_newtable(L);
    lua_setfield(L, LUA_REGISTRYINDEX, "DP_loaded_packages");
    lua_pushcfunction(L, require);
    lua_setglobal(L, "require");
    lua_pushcfunction(L, unrequire);
    lua_setglobal(L, "unrequire");
    return 0;
}

static int global_index(lua_State *L)
{
    lua_pushliteral(L, "Reference to undeclared global '");
    lua_pushvalue(L, 2);
    lua_pushliteral(L, "'");
    lua_concat(L, 3);
    return lua_error(L);
}

static int global_newindex(lua_State *L)
{
    lua_pushliteral(L, "Attempt to declare global '");
    lua_pushvalue(L, 2);
    lua_pushliteral(L, "'");
    lua_concat(L, 3);
    return lua_error(L);
}

static int init_global_environment(lua_State *L)
{
    lua_pushglobaltable(L);
    lua_newtable(L);
    lua_pushcfunction(L, global_index);
    lua_setfield(L, -2, "__index");
    lua_pushcfunction(L, global_newindex);
    lua_setfield(L, -2, "__newindex");
    lua_setmetatable(L, -2);
    return 0;
}

int DP_lua_client_init(lua_State *L);
int DP_lua_document_init(lua_State *L);
int DP_lua_message_init(lua_State *L);
int DP_lua_ui_init(lua_State *L);

static int init_bindings(lua_State *L)
{
    static const lua_CFunction init_funcs[] = {
        init_lua_libs,       init_app_funcs,          init_imgui,
        init_package,        DP_lua_client_init,      DP_lua_document_init,
        DP_lua_message_init, init_global_environment, DP_lua_ui_init,
    };
    lua_pushcfunction(L, init_lua_app_state);
    lua_pushvalue(L, 1);
    lua_call(L, 1, 0);
    for (size_t i = 0; i < DP_ARRAY_LENGTH(init_funcs); ++i) {
        lua_pushcfunction(L, init_funcs[i]);
        lua_call(L, 0, 0);
    }
    return 0;
}

static void handle_warn(void *user, const char *msg, int tocont)
{
    DP_LuaWarnBuffer *wb = user;
    size_t used = wb->used;
    if (tocont) {
        size_t length = strlen(msg);
        size_t needed = used + length;
        if (wb->capacity < needed) {
            wb->buffer = DP_realloc(wb->buffer, needed);
            wb->capacity = needed;
        }
        memcpy(wb->buffer + used, msg, length);
        wb->used += length;
    }
    else if (used == 0) {
        DP_warn("Lua: %s", msg);
    }
    else {
        DP_warn("Lua: %.*s%s", DP_size_to_int(used), wb->buffer, msg);
        wb->used = 0;
    }
}


bool DP_lua_bindings_init(lua_State *L, DP_App *app, DP_LuaWarnBuffer *wb)
{
    DP_ASSERT(L);
    DP_ASSERT(app);
    DP_ASSERT(wb);
    lua_setwarnf(L, handle_warn, wb);
    lua_pushcfunction(L, init_bindings);
    lua_pushlightuserdata(L, app);
    if (lua_pcall(L, 1, 0, 0) == LUA_OK) {
        return true;
    }
    else {
        DP_error_set("Error loading Lua bindings: %s", lua_tostring(L, -1));
        lua_pop(L, 1);
        return false;
    }
}


DP_LuaAppState *DP_lua_app_state(lua_State *L)
{
    return *(DP_LuaAppState **)lua_getextraspace(L);
}

DP_App *DP_lua_app(lua_State *L)
{
    return DP_lua_app_state(L)->app;
}


static int call_lua_app_new(lua_State *L)
{
    lua_getglobal(L, "require");
    lua_pushstring(L, "app");
    lua_call(L, 1, 1);
    lua_getfield(L, -1, "new");
    lua_pushvalue(L, -2);
    lua_call(L, 1, 1);
    return 1;
}

int DP_lua_app_new(lua_State *L)
{
    lua_pushcfunction(L, call_lua_app_new);
    if (lua_pcall(L, 0, 1, 0) == LUA_OK) {
        return luaL_ref(L, LUA_REGISTRYINDEX);
    }
    else {
        DP_error_set("Error loading Lua app: %s", lua_tostring(L, -1));
        lua_pop(L, 1);
        return LUA_NOREF;
    }
}

static void call_app_method(lua_State *L, int ref, const char *method)
{
    lua_pushcfunction(L, error_to_trace);
    int msgh = lua_gettop(L);
    lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
    lua_getfield(L, -1, method);
    lua_pushvalue(L, -2);
    if (lua_pcall(L, 1, 0, msgh) == LUA_OK) {
        lua_pop(L, 2);
    }
    else {
        DP_warn("Error in Lua %s: %s", method, lua_tostring(L, -1));
        lua_pop(L, 3);
    }
}

void DP_lua_app_free(lua_State *L, int ref)
{
    if (ref != LUA_NOREF) {
        call_app_method(L, ref, "dispose");
        lua_setwarnf(L, NULL, NULL);
        luaL_unref(L, LUA_REGISTRYINDEX, ref);
    }
}

void DP_lua_app_handle_events(lua_State *L, int ref, DP_LayerPropsList *lpl)
{
    move_client_events_to_lua(L, ref);
    if (lpl) {
        create_layer_props_event(L, ref, lpl);
    }
    call_app_method(L, ref, "handle_events");
}

#ifdef DRAWDANCE_IMGUI
void DP_lua_imgui_stack_clear(void);

void DP_lua_app_prepare_gui(lua_State *L, int ref)
{
    call_app_method(L, ref, "prepare_gui");
    DP_lua_imgui_stack_clear();
}
#endif


void DP_lua_warn_buffer_dispose(DP_LuaWarnBuffer *wb)
{
    if (wb) {
        DP_free(wb->buffer);
    }
}
